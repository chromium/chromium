# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import dataclasses
import os
import re
from typing import List
from typing import Optional

import models
import type_resolver


@dataclasses.dataclass
class ParsedMethod:
  name: str
  return_type: str
  params: List[models.Param]
  native_class_name: str


@dataclasses.dataclass
class ParsedFile:
  filename: str
  java_class: models.JavaClass
  type_resolver: type_resolver.TypeResolver
  proxy_methods: List[ParsedMethod]
  non_proxy_natives: list  # [jni_generator.NativeMethod]
  called_by_natives: list  # [jni_generator.CalledByNative]
  proxy_interface: Optional[models.JavaClass] = None
  module_name: Optional[str] = None  # E.g. @NativeMethods("module_name")
  jni_namespace: Optional[str] = None  # E.g. @JNINamespace("content")


@dataclasses.dataclass
class _ParsedProxyNatives:
  interface_name: str
  visibility: str
  module_name: str
  methods: List[ParsedMethod]


# Match single line comments, multiline comments, character literals, and
# double-quoted strings.
_COMMENT_REMOVER_REGEX = re.compile(
    r'//.*?$|/\*.*?\*/|\'(?:\\.|[^\\\'])*\'|"(?:\\.|[^\\"])*"',
    re.DOTALL | re.MULTILINE)


def _remove_comments(contents):
  # We need to support both inline and block comments, and we need to handle
  # strings that contain '//' or '/*'.
  def replacer(match):
    # Replace matches that are comments with nothing; return literals/strings
    # unchanged.
    s = match.group(0)
    if s.startswith('/'):
      return ''
    else:
      return s

  return _COMMENT_REMOVER_REGEX.sub(replacer, contents)


_PACKAGE_REGEX = re.compile('^package\s+(\S+?);', flags=re.MULTILINE)


def _parse_package(contents):
  match = _PACKAGE_REGEX.search(contents)
  if not match:
    raise SyntaxError('Unable to find "package" line')
  return match.group(1)


_CLASSES_REGEX = re.compile(
    r'^(.*?)(?:\b(public|protected|private)?\b)\s*'
    r'(?:\b(?:static|abstract|final|sealed)\s+)*'
    r'\b(?:class|interface|enum)\s+(\w+?)\b[^"]*?$',
    flags=re.MULTILINE)


# Does not handle doubly-nested classes.
def _parse_java_classes(contents):
  package = _parse_package(contents).replace('.', '/')
  outer_class = None
  nested_classes = []
  for m in _CLASSES_REGEX.finditer(contents):
    preamble, visibility, class_name = m.groups()
    # Ignore annoations like @Foo("contains the words class Bar")
    if preamble.count('"') % 2 != 0:
      continue
    if outer_class is None:
      outer_class = models.JavaClass(f'{package}/{class_name}', visibility)
    else:
      nested_classes.append(outer_class.make_nested(class_name, visibility))

  if outer_class is None:
    raise SyntaxError('No classes found.')

  return outer_class, nested_classes


def parse_javap_signature(signature_line):
  prefix = 'Signature: '
  index = signature_line.find(prefix)
  if index == -1:
    prefix = 'descriptor: '
    index = signature_line.index(prefix)
  return '"%s"' % signature_line[index + len(prefix):]


def strip_generics(value):
  """Strips Java generics from a string."""
  nest_level = 0  # How deeply we are nested inside the generics.
  start_index = 0  # Starting index of the last non-generic region.
  out = []

  for i, c in enumerate(value):
    if c == '<':
      if nest_level == 0:
        out.append(value[start_index:i])
      nest_level += 1
    elif c == '>':
      start_index = i + 1
      nest_level -= 1
  out.append(value[start_index:])
  return ''.join(out)


def parse_param_list(line, from_javap=False):
  """Parses the params into a list of Param objects."""
  if not line:
    return []
  ret = []
  line = strip_generics(line)
  for p in line.split(','):
    items = p.split()

    if 'final' in items:
      items.remove('final')

    # Remove @Annotations from parameters.
    annotations = []
    while items[0].startswith('@'):
      annotations.append(items[0])
      del items[0]

    datatype = items[0]
    # Handle varargs.
    if datatype.endswith('...'):
      datatype = datatype[:-3] + '[]'

    if from_javap:
      datatype = datatype.replace('.', '/')

    name = items[1] if len(items) > 1 else 'p%s' % len(ret)

    ret.append(
        models.Param(annotations=annotations, datatype=datatype, name=name))

  return ret


_NATIVE_PROXY_EXTRACTION_REGEX = re.compile(
    r'@NativeMethods(?:\(\s*"(?P<module_name>\w+)"\s*\))?[\S\s]+?'
    r'(?P<visibility>public)?\binterface\s*'
    r'(?P<interface_name>\w*)\s*(?P<interface_body>{(\s*.*)+?\s*})')

# Matches on method declarations unlike _EXTRACT_NATIVES_REGEX
# doesn't require name to be prefixed with native, and does not
# require a native qualifier.
_EXTRACT_METHODS_REGEX = re.compile(
    r'(@NativeClassQualifiedName\(\"(?P<native_class_name>\S*?)\"\)\s*)?'
    r'(?P<qualifiers>'
    r'((public|private|static|final|abstract|protected|native)\s*)*)\s+'
    r'(?P<return_type>\S*)\s+'
    r'(?P<name>\w+)\((?P<params>.*?)\);',
    flags=re.DOTALL)


def _parse_proxy_natives(contents):
  matches = list(_NATIVE_PROXY_EXTRACTION_REGEX.finditer(contents))
  if not matches:
    return None
  if len(matches) > 1:
    raise SyntaxError(
        'Multiple @NativeMethod interfaces in one class is not supported.')

  match = matches[0]
  ret = _ParsedProxyNatives(interface_name=match.group('interface_name'),
                            visibility=match.group('visibility') == 'public',
                            module_name=match.group('module_name'),
                            methods=[])
  interface_body = match.group('interface_body')

  for match in _EXTRACT_METHODS_REGEX.finditer(interface_body):
    params = parse_param_list(match.group('params'))
    ret.methods.append(
        ParsedMethod(name=match.group('name'),
                     return_type=match.group('return_type'),
                     params=params,
                     native_class_name=match.group('native_class_name')))
  if not ret.methods:
    raise SyntaxError('Found no methods within @NativeMethod interface.')
  return ret


_IMPORT_REGEX = re.compile(r'^import\s+([^\s*]+);', flags=re.MULTILINE)
_IMPORT_CLASS_NAME_REGEX = re.compile(r'^(.*?)\.([A-Z].*)')


def _parse_imports(contents):
  # Regex skips static imports as well as wildcard imports.
  names = _IMPORT_REGEX.findall(contents)
  for name in names:
    m = _IMPORT_CLASS_NAME_REGEX.match(name)
    if m:
      package, class_name = m.groups()
      yield models.JavaClass(
          package.replace('.', '/') + '/' + class_name.replace('.', '$'))


_JNI_NAMESPACE_REGEX = re.compile('@JNINamespace\("(.*?)"\)')


def _parse_jni_namespace(contents):
  m = _JNI_NAMESPACE_REGEX.findall(contents)
  if not m:
    return ''
  if len(m) > 1:
    raise SyntaxError('Found multiple @JNINamespace attributes.')
  return m[0]


def _do_parse(filename, *, package_prefix):
  assert not filename.endswith('.kt'), (
      f'Found {filename}, but Kotlin is not supported by JNI generator.')
  with open(filename) as f:
    contents = f.read()
  contents = _remove_comments(contents)

  outer_class, nested_classes = _parse_java_classes(contents)

  expected_name = os.path.splitext(os.path.basename(filename))[0]
  if outer_class.name != expected_name:
    raise SyntaxError(
        f'Found class "{outer_class.name}" but expected "{expected_name}".')

  if package_prefix:
    outer_class = outer_class.make_prefixed(package_prefix)
    nested_classes = [c.make_prefixed(package_prefix) for c in nested_classes]

  resolver = type_resolver.TypeResolver(outer_class)
  for java_class in _parse_imports(contents):
    resolver.add_import(java_class)
  for java_class in nested_classes:
    resolver.add_nested_class(java_class)

  parsed_proxy_natives = _parse_proxy_natives(contents)
  jni_namespace = _parse_jni_namespace(contents)

  # TODO(crbug.com/1406605): Remove circular dep.
  import jni_generator
  non_proxy_natives = jni_generator.ExtractNatives(contents, 'long')
  called_by_natives = jni_generator.ExtractCalledByNatives(resolver, contents)

  ret = ParsedFile(filename=filename,
                   jni_namespace=jni_namespace,
                   java_class=outer_class,
                   type_resolver=resolver,
                   proxy_methods=[],
                   non_proxy_natives=non_proxy_natives,
                   called_by_natives=called_by_natives)

  if parsed_proxy_natives:
    ret.module_name = parsed_proxy_natives.module_name
    ret.proxy_interface = outer_class.make_nested(
        parsed_proxy_natives.interface_name,
        visibility=parsed_proxy_natives.visibility)
    ret.proxy_methods = parsed_proxy_natives.methods

  return ret


def parse_java_file(filename, *, package_prefix=None):
  try:
    return _do_parse(filename, package_prefix=package_prefix)
  except SyntaxError as e:
    e.msg += f' (when parsing {filename})'
    raise
