# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import dataclasses
import os
import re
from typing import List
from typing import Optional

import java_types


@dataclasses.dataclass(order=True)
class ParsedMethod:
  name: str
  return_type: java_types.JavaType
  params: java_types.JavaParamList
  native_class_name: str


@dataclasses.dataclass
class ParsedFile:
  filename: str
  java_class: java_types.JavaClass
  type_resolver: java_types.TypeResolver
  proxy_methods: List[ParsedMethod]
  non_proxy_natives: list  # [jni_generator.NativeMethod]
  called_by_natives: list  # [jni_generator.CalledByNative]
  proxy_interface: Optional[java_types.JavaClass] = None
  proxy_visibility: Optional[str] = None
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


# This will also break lines with comparison operators, but we don't care.
_GENERICS_REGEX = re.compile(r'<[^<>\n]*>')


def remove_generics(value):
  """Strips Java generics from a string."""
  while True:
    ret = _GENERICS_REGEX.sub('', value)
    if len(ret) == len(value):
      return ret
    value = ret


_PACKAGE_REGEX = re.compile('^package\s+(\S+?);', flags=re.MULTILINE)


def _parse_package(contents):
  match = _PACKAGE_REGEX.search(contents)
  if not match:
    raise SyntaxError('Unable to find "package" line')
  return match.group(1)


_CLASSES_REGEX = re.compile(
    r'^(.*?)(?:\b(?:public|protected|private)?\b)\s*'
    r'(?:\b(?:static|abstract|final|sealed)\s+)*'
    r'\b(?:class|interface|enum)\s+(\w+?)\b[^"]*?$',
    flags=re.MULTILINE)


# Does not handle doubly-nested classes.
def _parse_java_classes(contents):
  package = _parse_package(contents).replace('.', '/')
  outer_class = None
  nested_classes = []
  for m in _CLASSES_REGEX.finditer(contents):
    preamble, class_name = m.groups()
    # Ignore annoations like @Foo("contains the words class Bar")
    if preamble.count('"') % 2 != 0:
      continue
    if outer_class is None:
      outer_class = java_types.JavaClass(f'{package}/{class_name}')
    else:
      nested_classes.append(outer_class.make_nested(class_name))

  if outer_class is None:
    raise SyntaxError('No classes found.')

  return outer_class, nested_classes


# Supports only @Foo and @Foo("value").
_ANNOTATION_REGEX = re.compile(r'@([\w.]+)(?:\(\s*"(.*?)\"\s*\))?\s*')


def _parse_annotations(value):
  annotations = {}
  last_idx = 0
  for m in _ANNOTATION_REGEX.finditer(value):
    annotations[m.group(1)] = m.group(2)
    last_idx = m.end()

  return annotations, value[last_idx:]


def parse_type(type_resolver, value):
  """Parses a string into a JavaType."""
  annotations, value = _parse_annotations(value)
  array_dimensions = 0
  while value[-2:] == '[]':
    array_dimensions += 1
    value = value[:-2]

  if value in java_types.PRIMITIVES:
    primitive_name = value
    java_class = None
  else:
    primitive_name = None
    java_class = type_resolver.resolve(value)

  return java_types.JavaType(array_dimensions=array_dimensions,
                             primitive_name=primitive_name,
                             java_class=java_class,
                             annotations=annotations)


_FINAL_REGEX = re.compile(r'\bfinal\s')


def parse_param_list(type_resolver,
                     value,
                     has_names=True) -> java_types.JavaParamList:
  if not value or value.isspace():
    return java_types.EMPTY_PARAM_LIST
  params = []
  value = _FINAL_REGEX.sub('', value)
  for param_str in value.split(','):
    param_str = param_str.strip()
    if has_names:
      param_str, _, param_name = param_str.rpartition(' ')
      param_str = param_str.rstrip()
    else:
      param_name = f'p{len(params)}'

    # Handle varargs.
    if param_str.endswith('...'):
      param_str = param_str[:-3] + '[]'

    param_type = parse_type(type_resolver, param_str)
    params.append(java_types.JavaParam(param_type, param_name))

  return java_types.JavaParamList(params)


_NATIVE_PROXY_EXTRACTION_REGEX = re.compile(
    r'@NativeMethods(?:\(\s*"(?P<module_name>\w+)"\s*\))?[\S\s]+?'
    r'(?P<visibility>public)?\s*\binterface\s*'
    r'(?P<interface_name>\w*)\s*{(?P<interface_body>(\s*.*)+?\s*)}')

# Matches on method declarations unlike _EXTRACT_NATIVES_REGEX
# doesn't require name to be prefixed with native, and does not
# require a native qualifier.
_EXTRACT_METHODS_REGEX = re.compile(r'\s*(.*?)\s+(\w+)\((.*?)\);',
                                    flags=re.DOTALL)

_PUBLIC_REGEX = re.compile(r'\bpublic\s')


def _parse_proxy_natives(type_resolver, contents):
  matches = list(_NATIVE_PROXY_EXTRACTION_REGEX.finditer(contents))
  if not matches:
    return None
  if len(matches) > 1:
    raise SyntaxError(
        'Multiple @NativeMethod interfaces in one class is not supported.')

  match = matches[0]
  ret = _ParsedProxyNatives(interface_name=match.group('interface_name'),
                            visibility=match.group('visibility'),
                            module_name=match.group('module_name'),
                            methods=[])
  interface_body = match.group('interface_body')

  for m in _EXTRACT_METHODS_REGEX.finditer(interface_body):
    preamble, name, params_part = m.groups()
    preamble = _PUBLIC_REGEX.sub('', preamble)
    annotations, return_type_part = _parse_annotations(preamble)
    params = parse_param_list(type_resolver, params_part)
    return_type = parse_type(type_resolver, return_type_part)
    ret.methods.append(
        ParsedMethod(
            name=name,
            return_type=return_type,
            params=params,
            native_class_name=annotations.get('NativeClassQualifiedName')))
  if not ret.methods:
    raise SyntaxError('Found no methods within @NativeMethod interface.')
  ret.methods.sort()
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
      yield java_types.JavaClass(
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
  contents = remove_generics(contents)

  outer_class, nested_classes = _parse_java_classes(contents)

  expected_name = os.path.splitext(os.path.basename(filename))[0]
  if outer_class.name != expected_name:
    raise SyntaxError(
        f'Found class "{outer_class.name}" but expected "{expected_name}".')

  if package_prefix:
    outer_class = outer_class.make_prefixed(package_prefix)
    nested_classes = [c.make_prefixed(package_prefix) for c in nested_classes]

  type_resolver = java_types.TypeResolver(outer_class)
  for java_class in _parse_imports(contents):
    type_resolver.add_import(java_class)
  for java_class in nested_classes:
    type_resolver.add_nested_class(java_class)

  parsed_proxy_natives = _parse_proxy_natives(type_resolver, contents)
  jni_namespace = _parse_jni_namespace(contents)

  # TODO(crbug.com/1406605): Remove circular dep.
  import jni_generator
  non_proxy_natives = jni_generator.ExtractNatives(type_resolver, contents)
  called_by_natives = jni_generator.ExtractCalledByNatives(
      type_resolver, contents)

  ret = ParsedFile(filename=filename,
                   jni_namespace=jni_namespace,
                   java_class=outer_class,
                   type_resolver=type_resolver,
                   proxy_methods=[],
                   non_proxy_natives=non_proxy_natives,
                   called_by_natives=called_by_natives)

  if parsed_proxy_natives:
    ret.module_name = parsed_proxy_natives.module_name
    ret.proxy_interface = outer_class.make_nested(
        parsed_proxy_natives.interface_name)
    ret.proxy_visibility = parsed_proxy_natives.visibility
    ret.proxy_methods = parsed_proxy_natives.methods

  return ret


def parse_java_file(filename, *, package_prefix=None):
  try:
    return _do_parse(filename, package_prefix=package_prefix)
  except SyntaxError as e:
    e.msg = (e.msg or '') + f' (when parsing {filename})'
    raise
