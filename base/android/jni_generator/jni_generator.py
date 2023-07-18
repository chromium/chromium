# Copyright 2012 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Entry point for "intermediates" command."""

import base64
import collections
import dataclasses
import hashlib
import os
import re
import shutil
from string import Template
import subprocess
import sys
import tempfile
import textwrap
import zipfile

_FILE_DIR = os.path.dirname(__file__)
_CHROMIUM_SRC = os.path.join(_FILE_DIR, os.pardir, os.pardir, os.pardir)
_BUILD_ANDROID_GYP = os.path.join(_CHROMIUM_SRC, 'build', 'android', 'gyp')

# Item 0 of sys.path is the directory of the main file; item 1 is PYTHONPATH
# (if set); item 2 is system libraries.
sys.path.insert(1, _BUILD_ANDROID_GYP)

from util import build_utils
import action_helpers  # build_utils adds //build to sys.path.
import zip_helpers  # build_utils adds //build to sys.path.

from codegen import placeholder_gen_jni_java
from codegen import proxy_impl_java
import common
import java_types
import parse
import proxy


_EXTRACT_NATIVES_REGEX = re.compile(
    r'(@NativeClassQualifiedName'
    r'\(\"(?P<native_class_name>\S*?)\"\)\s+)?'
    r'(?P<qualifiers>\w+\s\w+|\w+|\s+)\s*native\s+'
    r'(?P<return_type>\S*)\s+'
    r'(?P<name>native\w+)\((?P<params>.*?)\);', re.DOTALL)

# Use 100 columns rather than 80 because it makes many lines more readable.
_WRAP_LINE_LENGTH = 100
# WrapOutput() is fairly slow. Pre-creating TextWrappers helps a bit.
_WRAPPERS_BY_INDENT = [
    textwrap.TextWrapper(
        width=_WRAP_LINE_LENGTH,
        expand_tabs=False,
        replace_whitespace=False,
        subsequent_indent=' ' * (indent + 4),
        break_long_words=False) for indent in range(50)
]  # 50 chosen experimentally.

class ParseError(SyntaxError):
  """Exception thrown when we can't parse the input file."""

  def __init__(self, description, *context_lines):
    super().__init__(description)
    self.context_lines = context_lines

  def __str__(self):
    context = '\n'.join(self.context_lines)
    return '***\nERROR: %s\n\n%s\n***' % (self.msg, context)


class NativeMethod(object):
  """Describes a C/C++ method that is called by Java code"""

  def __init__(self, **kwargs):
    return_type = kwargs['return_type']
    self.signature = java_types.JavaSignature.from_params(
        return_type, kwargs['params'])
    self.is_proxy = kwargs.get('is_proxy', False)
    self.static = kwargs.get('static', self.is_proxy)

    self.name = kwargs['name']
    # Proxy methods don't have a native prefix so the first letter is
    # lowercase. But we still want the CPP declaration to use upper camel
    # case for the method name.
    self.cpp_name = common.capitalize(self.name)
    self.is_test_only = _NameIsTestOnly(self.name)

    if self.is_proxy:
      self.proxy_signature = self.signature.to_proxy()
      self.proxy_name, self.hashed_proxy_name = proxy.create_method_names(
          kwargs['java_class'], self.name, self.is_test_only)
      self.switch_num = None
    else:
      self.proxy_signature = self.signature

    first_param = self.params and self.params[0]
    if (first_param and first_param.java_type.is_primitive()
        and first_param.java_type.primitive_name == 'long'
        and first_param.name.startswith('native')):
      self.type = 'method'
      self.p0_type = kwargs.get('p0_type')
      if self.p0_type is None:
        self.p0_type = first_param.name[len('native'):]
        if kwargs.get('native_class_name'):
          self.p0_type = kwargs['native_class_name']
    else:
      self.type = 'function'
    self.method_id_var_name = kwargs.get('method_id_var_name', None)

  @property
  def return_type(self):
    return self.signature.return_type

  @property
  def proxy_return_type(self):
    return self.proxy_signature.return_type

  @property
  def params(self):
    return self.signature.param_list

  @property
  def proxy_params(self):
    return self.proxy_signature.param_list


class CalledByNative(object):
  """Describes a java method exported to c/c++"""

  def __init__(self, **kwargs):
    return_type = kwargs['return_type']
    self.system_class = kwargs['system_class']
    self.unchecked = kwargs['unchecked']
    self.static = kwargs['static']
    self.java_class_name = kwargs['java_class_name']
    self.name = kwargs['name']
    self.descriptor = kwargs.get('descriptor')
    self.signature = java_types.JavaSignature.from_params(
        return_type, kwargs['params'])
    self.method_id_var_name = kwargs.get('method_id_var_name', None)
    self.is_constructor = kwargs.get('is_constructor', False)
    self.env_call = GetEnvCall(self.is_constructor, self.static, return_type)
    self.static_cast = GetStaticCastForReturnType(return_type)

  @property
  def return_type(self):
    return self.signature.return_type

  @property
  def params(self):
    return self.signature.param_list


class ConstantField(object):

  def __init__(self, **kwargs):
    self.name = kwargs['name']
    self.value = kwargs['value']


def JavaTypeToCForDeclaration(java_type):
  """Wrap the C datatype in a JavaParamRef if required."""
  c_type = java_type.to_cpp()
  if java_type.is_primitive():
    return c_type
  return f'const base::android::JavaParamRef<{c_type}>&'


def JavaTypeToCForCalledByNativeParam(java_type):
  """Returns a C datatype to be when calling from native."""
  c_type = java_type.to_cpp()
  if java_type.is_primitive():
    if c_type == 'jint':
      return 'JniIntWrapper'
    return c_type
  return f'const base::android::JavaRef<{c_type}>&'


def _GetJNIFirstParam(native, for_declaration):
  c_type = 'jclass' if native.static else 'jobject'

  if for_declaration:
    c_type = f'const base::android::JavaParamRef<{c_type}>&'
  return [c_type + ' jcaller']


def _GetParamsInDeclaration(native):
  """Returns the params for the forward declaration.

  Args:
    native: the native dictionary describing the method.

  Returns:
    A string containing the params.
  """
  ret = [
      JavaTypeToCForDeclaration(p.java_type) + ' ' + p.name
      for p in native.params
  ]
  if not native.static:
    ret = _GetJNIFirstParam(native, True) + ret
  return ret


def GetParamsInStub(native):
  """Returns the params for the stub declaration.

  Args:
    native: the native dictionary describing the method.

  Returns:
    A string containing the params.
  """
  params = [p.java_type.to_cpp() + ' ' + p.name for p in native.params]
  params = _GetJNIFirstParam(native, False) + params
  return ',\n    '.join(params)


def _NameIsTestOnly(name):
  return name.endswith('ForTest') or name.endswith('ForTesting')


def ExtractNatives(type_resolver, contents):
  """Returns a list of dict containing information about a native method."""
  natives = []
  for match in _EXTRACT_NATIVES_REGEX.finditer(contents):
    return_type = parse.parse_type(type_resolver, match.group('return_type'))
    params = parse.parse_param_list(type_resolver, match.group('params'))
    native = NativeMethod(static='static' in match.group('qualifiers'),
                          native_class_name=match.group('native_class_name'),
                          return_type=return_type,
                          params=params,
                          name=match.group('name').replace('native', ''))
    natives.append(native)
  natives.sort(key=lambda x: (x.name, x.signature))
  return natives


def GetRegistrationFunctionName(fully_qualified_class):
  """Returns the register name with a given class."""
  return 'RegisterNative_' + common.escape_class_name(fully_qualified_class)


def GetStaticCastForReturnType(return_type):
  if return_type.is_primitive():
    return None
  ret = return_type.to_cpp()
  return None if ret == 'jobject' else ret


def GetEnvCall(is_constructor, is_static, return_type):
  """Maps the types availabe via env->Call__Method."""
  if is_constructor:
    return 'NewObject'
  if return_type.is_primitive():
    name = return_type.primitive_name
    call = common.capitalize(return_type.primitive_name)
  else:
    call = 'Object'
  if is_static:
    call = 'Static' + call
  return 'Call' + call + 'Method'


def MangledType(java_type):
  """Returns a mangled identifier for the datatype."""
  descriptor = java_type.to_descriptor()
  if len(descriptor) <= 2:
    return descriptor.replace('[', 'A')
  ret = []
  # TODO(agrieve): This is not adding an A for one-dimensional arrays.
  for i in range(1, len(descriptor)):
    c = descriptor[i]
    if c == '[':
      ret.append('A')
    elif c.isupper() or descriptor[i - 1] in '/L':
      ret.append(c.upper())
  return ''.join(ret)


def GetMangledMethodName(name, signature):
  """Returns a mangled method name for the given signature.

     The returned name can be used as a C identifier and will be unique for all
     valid overloads of the same method.

  Returns:
      A mangled name.
  """
  mangled_items = []
  for java_type in (signature.return_type, ) + signature.param_types:
    mangled_items.append(MangledType(java_type))
  mangled_name = name + '_'.join(mangled_items)
  assert re.match(r'[0-9a-zA-Z_]+', mangled_name)
  return mangled_name


def MangleCalledByNatives(called_by_natives):
  """Mangles all the overloads from the call_by_natives list."""
  method_counts = collections.defaultdict(
      lambda: collections.defaultdict(lambda: 0))
  for called_by_native in called_by_natives:
    java_class_name = called_by_native.java_class_name
    name = called_by_native.name
    method_counts[java_class_name][name] += 1
  for called_by_native in called_by_natives:
    java_class_name = called_by_native.java_class_name
    method_name = called_by_native.name
    method_id_var_name = method_name
    if method_counts[java_class_name][method_name] > 1:
      method_id_var_name = GetMangledMethodName(method_name,
                                                called_by_native.signature)
    called_by_native.method_id_var_name = method_id_var_name
  return called_by_natives


# Regex to match a string like "@CalledByNative public void foo(int bar)".
RE_CALLED_BY_NATIVE = re.compile(
    r'@CalledByNative((?P<Unchecked>(?:Unchecked)?|ForTesting))'
    r'(?:\("(?P<annotation>.*)"\))?'
    r'(?:\s+@\w+(?:\(.*\))?)*'  # Ignore any other annotations.
    r'\s+(?P<prefix>('
    r'(private|protected|public|static|abstract|final|default|synchronized)'
    r'\s*)*)'
    r'(?:\s*@\w+)?'  # Ignore annotations in return types.
    r'\s*(?P<return_type>\S*?)'
    r'\s*(?P<name>\w+)'
    r'\s*\((?P<params>[^\)]*)\)')


# Removes empty lines that are indented (i.e. start with 2x spaces).
def RemoveIndentedEmptyLines(string):
  return re.sub('^(?: {2})+$\n', '', string, flags=re.MULTILINE)


def ExtractCalledByNatives(type_resolver, contents):
  """Parses all methods annotated with @CalledByNative.

  Returns:
    A list of dict with information about the annotated methods.
    TODO(bulach): return a CalledByNative object.

  Raises:
    ParseError: if unable to parse.
  """
  called_by_natives = []
  for match in re.finditer(RE_CALLED_BY_NATIVE, contents):
    return_type = match.group('return_type')
    name = match.group('name')
    if not return_type:
      is_constructor = True
      return_type = name
      name = "Constructor"
    else:
      is_constructor = False

    return_type = parse.parse_type(type_resolver, return_type)
    params = parse.parse_param_list(type_resolver, match.group('params'))
    called_by_natives.append(
        CalledByNative(system_class=False,
                       unchecked='Unchecked' in match.group('Unchecked'),
                       static='static' in match.group('prefix'),
                       java_class_name=match.group('annotation') or '',
                       return_type=return_type,
                       params=params,
                       name=name,
                       is_constructor=is_constructor))
  # Check for any @CalledByNative occurrences that weren't matched.
  unmatched_lines = re.sub(RE_CALLED_BY_NATIVE, '', contents).split('\n')
  for line1, line2 in zip(unmatched_lines, unmatched_lines[1:]):
    if '@CalledByNative' in line1:
      raise ParseError('could not parse @CalledByNative method signature',
                       line1, line2)
  called_by_natives.sort(key=lambda x: (x.java_class_name, x.name, x.signature))
  return MangleCalledByNatives(called_by_natives)


def _ParseJavapDescriptor(line):
  prefix = 'descriptor: '
  index = line.index(prefix)
  return line[index + len(prefix):]


class JNIFromJavaP(object):
  """Uses 'javap' to parse a .class file and generate the JNI header file."""

  def __init__(self, contents, options):
    contents = parse.remove_generics(contents)
    lines = contents.splitlines()
    self.options = options
    for line in lines:
      m = re.match('.*?(?:public).*?(?:class|interface) (\S+?)(?: |\Z)', line)
      if m:
        fqn = m.group(1).split('<', 1)[0].replace('.', '/')
        self.java_class = java_types.JavaClass(fqn)
        break
    else:
      raise SyntaxError('Could not find java class in javap output')
    type_resolver = java_types.TypeResolver(self.java_class)
    self.type_resolver = type_resolver
    self.jni_namespace = options.namespace or 'JNI_' + self.java_class.name
    re_method = re.compile('(?P<prefix>.*?)(?P<return_type>\S+?) (?P<name>\w+?)'
                           '\((?P<params>.*?)\)')
    self.called_by_natives = []
    for lineno, content in enumerate(lines[2:], 2):
      match = re.match(re_method, content)
      if not match:
        continue
      return_type = parse.parse_type(type_resolver, match.group('return_type'))
      params = parse.parse_param_list(type_resolver,
                                      match.group('params'),
                                      has_names=False)
      descriptor = _ParseJavapDescriptor(lines[lineno + 1])

      self.called_by_natives.append(
          CalledByNative(system_class=True,
                         unchecked=options.unchecked_exceptions,
                         static='static' in match.group('prefix'),
                         java_class_name='',
                         return_type=return_type,
                         params=params,
                         name=match.group('name'),
                         descriptor=descriptor))
    re_constructor = re.compile('(.*?)public ' +
                                self.java_class.full_name_with_dots +
                                '\((?P<params>.*?)\)')
    for lineno, content in enumerate(lines[2:], 2):
      match = re.match(re_constructor, content)
      if not match:
        continue
      return_type = java_types.JavaType(java_class=self.java_class)
      params = parse.parse_param_list(type_resolver,
                                      match.group('params'),
                                      has_names=False)
      descriptor = _ParseJavapDescriptor(lines[lineno + 1])

      self.called_by_natives.append(
          CalledByNative(system_class=True,
                         unchecked=options.unchecked_exceptions,
                         static=False,
                         java_class_name='',
                         return_type=return_type,
                         params=params,
                         name='Constructor',
                         descriptor=descriptor,
                         is_constructor=True))
    self.called_by_natives = MangleCalledByNatives(self.called_by_natives)
    self.constant_fields = []
    re_constant_field = re.compile('.*?public static final int (?P<name>.*?);')
    re_constant_field_value = re.compile(
        '.*?Constant(Value| value): int (?P<value>(-*[0-9]+)?)')
    for lineno, content in enumerate(lines[2:], 2):
      match = re.match(re_constant_field, content)
      if not match:
        continue
      value = re.match(re_constant_field_value, lines[lineno + 2])
      if not value:
        value = re.match(re_constant_field_value, lines[lineno + 3])
      if value:
        self.constant_fields.append(
            ConstantField(name=match.group('name'), value=value.group('value')))

  def GetContent(self):
    # We pass in an empty string for the module (which will make the JNI use the
    # base module's files) for all javap-derived JNI. There may be a way to get
    # the module from a jar file, but it's not needed right now.
    generator = InlHeaderFileGenerator('', self.jni_namespace, self.java_class,
                                       [], self.called_by_natives,
                                       self.constant_fields, self.type_resolver,
                                       self.options)
    return generator.GetContent()

  @staticmethod
  def CreateFromClass(class_file, options):
    class_name = os.path.splitext(os.path.basename(class_file))[0]
    javap_path = os.path.abspath(options.javap)
    p = subprocess.Popen(
        args=[javap_path, '-c', '-verbose', '-s', class_name],
        cwd=os.path.dirname(class_file),
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        universal_newlines=True)
    stdout, _ = p.communicate()
    jni_from_javap = JNIFromJavaP(stdout, options)
    return jni_from_javap

class JNIFromJavaSource(object):
  """Uses the given java source file to generate the JNI header file."""

  def __init__(self, parsed_file, options):
    self.options = options
    self.filename = parsed_file.filename
    self.java_class = parsed_file.java_class
    self.type_resolver = parsed_file.type_resolver
    self.jni_namespace = parsed_file.jni_namespace or options.namespace
    self.module_name = parsed_file.module_name
    self.proxy_interface = parsed_file.proxy_interface
    self.proxy_visibility = parsed_file.proxy_visibility

    proxy_natives = []
    for parsed_method in parsed_file.proxy_methods:
      proxy_natives.append(
          NativeMethod(is_proxy=True,
                       java_class=self.java_class,
                       name=parsed_method.name,
                       return_type=parsed_method.return_type,
                       params=parsed_method.params,
                       native_class_name=parsed_method.native_class_name))

    self.natives = proxy_natives + parsed_file.non_proxy_natives
    self.called_by_natives = parsed_file.called_by_natives

  @property
  def proxy_natives(self):
    return [n for n in self.natives if n.is_proxy]

  @property
  def non_proxy_natives(self):
    return [n for n in self.natives if not n.is_proxy]

  def RemoveTestOnlyNatives(self):
    self.natives = [n for n in self.natives if not n.is_test_only]

  def GetContent(self):
    generator = InlHeaderFileGenerator(self.module_name, self.jni_namespace,
                                       self.java_class, self.natives,
                                       self.called_by_natives, [],
                                       self.type_resolver, self.options)
    return generator.GetContent()

  @staticmethod
  def CreateFromFile(filename, options):
    parsed_file = parse.parse_java_file(filename,
                                        package_prefix=options.package_prefix)
    return JNIFromJavaSource(parsed_file, options)


class HeaderFileGeneratorHelper(object):
  """Include helper methods for header generators."""

  def __init__(self,
               class_name,
               module_name,
               fully_qualified_class,
               use_proxy_hash,
               package_prefix,
               split_name=None,
               enable_jni_multiplexing=False):
    self.class_name = class_name
    self.module_name = module_name
    self.fully_qualified_class = fully_qualified_class
    self.use_proxy_hash = use_proxy_hash
    self.package_prefix = package_prefix
    self.split_name = split_name
    self.enable_jni_multiplexing = enable_jni_multiplexing
    self.gen_jni_class = proxy.get_gen_jni_class(short=use_proxy_hash
                                                 or enable_jni_multiplexing,
                                                 name_prefix=module_name,
                                                 package_prefix=package_prefix)

  def GetStubName(self, native):
    """Return the name of the stub function for this native method.

    Args:
      native: the native dictionary describing the method.

    Returns:
      A string with the stub function name (used by the JVM).
    """
    if native.is_proxy:
      if self.use_proxy_hash:
        method_name = common.escape_class_name(native.hashed_proxy_name)
      else:
        method_name = common.escape_class_name(native.proxy_name)
      return 'Java_%s_%s' % (common.escape_class_name(
          self.gen_jni_class.full_name_with_slashes), method_name)

    template = Template('Java_${JAVA_NAME}_native${NAME}')

    java_name = self.fully_qualified_class

    values = {
        'NAME': native.cpp_name,
        'JAVA_NAME': common.escape_class_name(java_name)
    }
    return template.substitute(values)

  def GetUniqueClasses(self, origin):
    ret = collections.OrderedDict()
    for entry in origin:
      if isinstance(entry, NativeMethod) and entry.is_proxy:
        short_name = self.use_proxy_hash or self.enable_jni_multiplexing
        ret[self.gen_jni_class.name] = self.gen_jni_class.full_name_with_slashes
        continue
      ret[self.class_name] = self.fully_qualified_class

      class_name = self.class_name
      jni_class_path = self.fully_qualified_class
      if isinstance(entry, CalledByNative) and entry.java_class_name:
        class_name = entry.java_class_name
        jni_class_path = self.fully_qualified_class + '$' + class_name
      ret[class_name] = jni_class_path
    return ret

  def GetClassPathLines(self, classes, declare_only=False):
    """Returns the ClassPath constants."""
    ret = []
    if declare_only:
      template = Template("""
extern const char kClassPath_${JAVA_CLASS}[];
""")
    else:
      template = Template("""
JNI_REGISTRATION_EXPORT extern const char kClassPath_${JAVA_CLASS}[];
const char kClassPath_${JAVA_CLASS}[] = \
"${JNI_CLASS_PATH}";
""")

    for full_clazz in classes.values():
      values = {
          'JAVA_CLASS': common.escape_class_name(full_clazz),
          'JNI_CLASS_PATH': full_clazz,
      }
      # Since all proxy methods use the same class, defining this in every
      # header file would result in duplicated extern initializations.
      if full_clazz != self.gen_jni_class.full_name_with_slashes:
        ret += [template.substitute(values)]

    class_getter = """\
#ifndef ${JAVA_CLASS}_clazz_defined
#define ${JAVA_CLASS}_clazz_defined
inline jclass ${JAVA_CLASS}_clazz(JNIEnv* env) {
  return base::android::LazyGetClass(env, kClassPath_${JAVA_CLASS}, \
${MAYBE_SPLIT_NAME_ARG}&g_${JAVA_CLASS}_clazz);
}
#endif
"""
    if declare_only:
      template = Template("""\
extern std::atomic<jclass> g_${JAVA_CLASS}_clazz;
""" + class_getter)
    else:
      template = Template("""\
// Leaking this jclass as we cannot use LazyInstance from some threads.
JNI_REGISTRATION_EXPORT std::atomic<jclass> g_${JAVA_CLASS}_clazz(nullptr);
""" + class_getter)

    for full_clazz in classes.values():
      values = {
          'JAVA_CLASS':
          common.escape_class_name(full_clazz),
          'MAYBE_SPLIT_NAME_ARG':
          (('"%s", ' % self.split_name) if self.split_name else '')
      }
      # Since all proxy methods use the same class, defining this in every
      # header file would result in duplicated extern initializations.
      if full_clazz != self.gen_jni_class.full_name_with_slashes:
        ret += [template.substitute(values)]

    return ''.join(ret)


class InlHeaderFileGenerator(object):
  """Generates an inline header file for JNI integration."""

  def __init__(self, module_name, namespace, java_class, natives,
               called_by_natives, constant_fields, type_resolver, options):
    self.namespace = namespace
    self.java_class = java_class
    self.class_name = java_class.name
    self.natives = natives
    self.called_by_natives = called_by_natives
    self.header_guard = java_class.full_name_with_slashes.replace('/',
                                                                  '_') + '_JNI'
    self.constant_fields = constant_fields
    self.type_resolver = type_resolver
    self.options = options
    self.helper = HeaderFileGeneratorHelper(
        java_class.name,
        module_name,
        self.java_class.full_name_with_slashes,
        self.options.use_proxy_hash,
        self.options.package_prefix,
        split_name=self.options.split_name,
        enable_jni_multiplexing=self.options.enable_jni_multiplexing)

  def GetContent(self):
    """Returns the content of the JNI binding file."""
    template = Template("""\
// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


// This file is autogenerated by
//     ${SCRIPT_NAME}
// For
//     ${FULLY_QUALIFIED_CLASS}

#ifndef ${HEADER_GUARD}
#define ${HEADER_GUARD}

#include <jni.h>

${INCLUDES}

// Step 1: Forward declarations.
$CLASS_PATH_DEFINITIONS

// Step 2: Constants (optional).

$CONSTANT_FIELDS\

// Step 3: Method stubs.
$METHOD_STUBS

#endif  // ${HEADER_GUARD}
""")
    values = {
        'SCRIPT_NAME': GetScriptName(),
        'FULLY_QUALIFIED_CLASS': self.java_class.full_name_with_slashes,
        'CLASS_PATH_DEFINITIONS': self.GetClassPathDefinitionsString(),
        'CONSTANT_FIELDS': self.GetConstantFieldsString(),
        'METHOD_STUBS': self.GetMethodStubsString(),
        'HEADER_GUARD': self.header_guard,
        'INCLUDES': self.GetIncludesString(),
    }
    open_namespace = self.GetOpenNamespaceString()
    if open_namespace:
      close_namespace = self.GetCloseNamespaceString()
      values['METHOD_STUBS'] = '\n'.join(
          [open_namespace, values['METHOD_STUBS'], close_namespace])

      constant_fields = values['CONSTANT_FIELDS']
      if constant_fields:
        values['CONSTANT_FIELDS'] = '\n'.join(
            [open_namespace, constant_fields, close_namespace])

    return WrapOutput(template.substitute(values))

  def GetClassPathDefinitionsString(self):
    classes = self.helper.GetUniqueClasses(self.called_by_natives)
    classes.update(self.helper.GetUniqueClasses(self.natives))
    return self.helper.GetClassPathLines(classes)

  def GetConstantFieldsString(self):
    if not self.constant_fields:
      return ''
    ret = ['enum Java_%s_constant_fields {' % self.java_class.name]
    for c in self.constant_fields:
      ret += ['  %s = %s,' % (c.name, c.value)]
    ret += ['};', '']
    return '\n'.join(ret)

  def GetMethodStubsString(self):
    """Returns the code corresponding to method stubs."""
    ret = []
    for native in self.natives:
      ret += [self.GetNativeStub(native)]
    ret += self.GetLazyCalledByNativeMethodStubs()
    return '\n'.join(ret)

  def GetLazyCalledByNativeMethodStubs(self):
    return [
        self.GetLazyCalledByNativeMethodStub(called_by_native)
        for called_by_native in self.called_by_natives
    ]

  def GetIncludesString(self):
    if not self.options.extra_includes:
      return ''
    includes = self.options.extra_includes
    return '\n'.join('#include "%s"' % x for x in includes) + '\n'

  def GetOpenNamespaceString(self):
    if self.namespace:
      all_namespaces = [
          'namespace %s {' % ns for ns in self.namespace.split('::')
      ]
      return '\n'.join(all_namespaces) + '\n'
    return ''

  def GetCloseNamespaceString(self):
    if self.namespace:
      all_namespaces = [
          '}  // namespace %s' % ns for ns in self.namespace.split('::')
      ]
      all_namespaces.reverse()
      return '\n' + '\n'.join(all_namespaces)
    return ''

  def GetCalledByNativeParamsInDeclaration(self, called_by_native):
    return ',\n    '.join([
        JavaTypeToCForCalledByNativeParam(p.java_type) + ' ' + p.name
        for p in called_by_native.params
    ])

  def GetJavaParamRefForCall(self, c_type, name):
    return Template(
        'base::android::JavaParamRef<${TYPE}>(env, ${NAME})').substitute({
            'TYPE':
            c_type,
            'NAME':
            name,
        })

  def GetImplementationMethodName(self, native):
    return 'JNI_%s_%s' % (self.java_class.name, native.cpp_name)

  def GetNativeStub(self, native):
    is_method = native.type == 'method'

    if is_method:
      params = native.params[1:]
    else:
      params = native.params

    params_in_call = ['env']
    if not native.static:
      # Add jcaller param.
      params_in_call.append(self.GetJavaParamRefForCall('jobject', 'jcaller'))

    for p in params:
      if p.java_type.is_primitive():
        params_in_call.append(p.name)
      else:
        c_type = p.java_type.to_cpp()
        params_in_call.append(self.GetJavaParamRefForCall(c_type, p.name))

    params_in_declaration = _GetParamsInDeclaration(native)
    params_in_call = ', '.join(params_in_call)

    return_type = native.return_type.to_cpp()
    return_declaration = return_type
    post_call = ''
    if not native.return_type.is_primitive():
      post_call = '.Release()'
      return_declaration = (
          'base::android::ScopedJavaLocalRef<' + return_type + '>')
    profiling_entered_native = ''
    if self.options.enable_profiling:
      profiling_entered_native = '  JNI_LINK_SAVED_FRAME_POINTER;\n'

    values = {
        'RETURN': return_type,
        'RETURN_DECLARATION': return_declaration,
        'NAME': native.cpp_name,
        'IMPL_METHOD_NAME': self.GetImplementationMethodName(native),
        'PARAMS': ',\n    '.join(params_in_declaration),
        'PARAMS_IN_STUB': GetParamsInStub(native),
        'PARAMS_IN_CALL': params_in_call,
        'POST_CALL': post_call,
        'STUB_NAME': self.helper.GetStubName(native),
        'PROFILING_ENTERED_NATIVE': profiling_entered_native,
    }

    namespace_qual = self.namespace + '::' if self.namespace else ''
    if is_method:
      optional_error_return = native.return_type.to_cpp_default_value()
      if optional_error_return:
        optional_error_return = ', ' + optional_error_return
      values.update({
          'OPTIONAL_ERROR_RETURN': optional_error_return,
          'PARAM0_NAME': native.params[0].name,
          'P0_TYPE': native.p0_type,
      })
      template = Template("""\
JNI_GENERATOR_EXPORT ${RETURN} ${STUB_NAME}(
    JNIEnv* env,
    ${PARAMS_IN_STUB}) {
${PROFILING_ENTERED_NATIVE}\
  ${P0_TYPE}* native = reinterpret_cast<${P0_TYPE}*>(${PARAM0_NAME});
  CHECK_NATIVE_PTR(env, jcaller, native, "${NAME}"${OPTIONAL_ERROR_RETURN});
  return native->${NAME}(${PARAMS_IN_CALL})${POST_CALL};
}
""")
    else:
      if values['PARAMS']:
        values['PARAMS'] = ', ' + values['PARAMS']
      template = Template("""\
static ${RETURN_DECLARATION} ${IMPL_METHOD_NAME}(JNIEnv* env${PARAMS});

JNI_GENERATOR_EXPORT ${RETURN} ${STUB_NAME}(
    JNIEnv* env,
    ${PARAMS_IN_STUB}) {
${PROFILING_ENTERED_NATIVE}\
  return ${IMPL_METHOD_NAME}(${PARAMS_IN_CALL})${POST_CALL};
}
""")

    return RemoveIndentedEmptyLines(template.substitute(values))

  def GetArgument(self, param):
    if param.java_type.is_primitive():
      if param.java_type.primitive_name == 'int':
        return f'as_jint({param.name})'
      return param.name
    return f'{param.name}.obj()'

  def GetCalledByNativeValues(self, called_by_native):
    """Fills in necessary values for the CalledByNative methods."""
    java_class_only = called_by_native.java_class_name or self.java_class.name
    java_class = self.java_class.full_name_with_slashes
    if called_by_native.java_class_name:
      java_class += '$' + called_by_native.java_class_name

    if called_by_native.static or called_by_native.is_constructor:
      first_param_in_declaration = ''
      first_param_in_call = 'clazz'
    else:
      first_param_in_declaration = (
          ', const base::android::JavaRef<jobject>& obj')
      first_param_in_call = 'obj.obj()'
    params_in_declaration = self.GetCalledByNativeParamsInDeclaration(
        called_by_native)
    if params_in_declaration:
      params_in_declaration = ', ' + params_in_declaration
    params_in_call = ', '.join(
        self.GetArgument(p) for p in called_by_native.params)
    if params_in_call:
      params_in_call = ', ' + params_in_call
    pre_call = ''
    post_call = ''
    if called_by_native.static_cast:
      pre_call = 'static_cast<%s>(' % called_by_native.static_cast
      post_call = ')'
    check_exception = 'Unchecked'
    method_id_member_name = 'call_context.method_id'
    if not called_by_native.unchecked:
      check_exception = 'Checked'
      method_id_member_name = 'call_context.base.method_id'
    return_type = called_by_native.return_type.to_cpp()
    optional_error_return = called_by_native.return_type.to_cpp_default_value()
    if optional_error_return:
      optional_error_return = ', ' + optional_error_return
    return_declaration = ''
    return_clause = ''
    if return_type != 'void':
      pre_call = ' ' + pre_call
      return_declaration = return_type + ' ret ='
      if called_by_native.return_type.is_primitive():
        return_clause = 'return ret;'
      else:
        return_type = 'base::android::ScopedJavaLocalRef<' + return_type + '>'
        return_clause = 'return ' + return_type + '(env, ret);'
    profiling_leaving_native = ''
    if self.options.enable_profiling:
      profiling_leaving_native = '  JNI_SAVE_FRAME_POINTER;\n'
    jni_name = called_by_native.name
    if called_by_native.is_constructor:
      jni_name = '<init>'
    if called_by_native.descriptor:
      jni_descriptor = called_by_native.descriptor
    else:
      sig = called_by_native.signature
      if called_by_native.is_constructor:
        sig = dataclasses.replace(sig, return_type=java_types.VOID)
      jni_descriptor = sig.to_descriptor()
    java_name_full = java_class.replace('/', '.') + '.' + jni_name
    return {
        'JAVA_CLASS_ONLY': java_class_only,
        'JAVA_CLASS': common.escape_class_name(java_class),
        'RETURN_TYPE': return_type,
        'OPTIONAL_ERROR_RETURN': optional_error_return,
        'RETURN_DECLARATION': return_declaration,
        'RETURN_CLAUSE': return_clause,
        'FIRST_PARAM_IN_DECLARATION': first_param_in_declaration,
        'PARAMS_IN_DECLARATION': params_in_declaration,
        'PRE_CALL': pre_call,
        'POST_CALL': post_call,
        'ENV_CALL': called_by_native.env_call,
        'FIRST_PARAM_IN_CALL': first_param_in_call,
        'PARAMS_IN_CALL': params_in_call,
        'CHECK_EXCEPTION': check_exception,
        'PROFILING_LEAVING_NATIVE': profiling_leaving_native,
        'JNI_NAME': jni_name,
        'JNI_DESCRIPTOR': jni_descriptor,
        'METHOD_ID_MEMBER_NAME': method_id_member_name,
        'METHOD_ID_VAR_NAME': called_by_native.method_id_var_name,
        'METHOD_ID_TYPE': 'STATIC' if called_by_native.static else 'INSTANCE',
        'JAVA_NAME_FULL': java_name_full,
    }

  def GetLazyCalledByNativeMethodStub(self, called_by_native):
    """Returns a string."""
    function_signature_template = Template("""\
static ${RETURN_TYPE} Java_${JAVA_CLASS_ONLY}_${METHOD_ID_VAR_NAME}(\
JNIEnv* env${FIRST_PARAM_IN_DECLARATION}${PARAMS_IN_DECLARATION})""")
    function_header_template = Template("""\
${FUNCTION_SIGNATURE} {""")
    function_header_with_unused_template = Template("""\
[[maybe_unused]] ${FUNCTION_SIGNATURE};
${FUNCTION_SIGNATURE} {""")
    template = Template("""
static std::atomic<jmethodID> g_${JAVA_CLASS}_${METHOD_ID_VAR_NAME}(nullptr);
${FUNCTION_HEADER}
  jclass clazz = ${JAVA_CLASS}_clazz(env);
  CHECK_CLAZZ(env, ${FIRST_PARAM_IN_CALL},
      ${JAVA_CLASS}_clazz(env)${OPTIONAL_ERROR_RETURN});

  jni_generator::JniJavaCallContext${CHECK_EXCEPTION} call_context;
  call_context.Init<
      base::android::MethodID::TYPE_${METHOD_ID_TYPE}>(
          env,
          clazz,
          "${JNI_NAME}",
          "${JNI_DESCRIPTOR}",
          &g_${JAVA_CLASS}_${METHOD_ID_VAR_NAME});

${PROFILING_LEAVING_NATIVE}\
  ${RETURN_DECLARATION}
     ${PRE_CALL}env->${ENV_CALL}(${FIRST_PARAM_IN_CALL},
          ${METHOD_ID_MEMBER_NAME}${PARAMS_IN_CALL})${POST_CALL};
  ${RETURN_CLAUSE}
}""")
    values = self.GetCalledByNativeValues(called_by_native)
    values['FUNCTION_SIGNATURE'] = (
        function_signature_template.substitute(values))
    if called_by_native.system_class:
      values['FUNCTION_HEADER'] = (
          function_header_with_unused_template.substitute(values))
    else:
      values['FUNCTION_HEADER'] = function_header_template.substitute(values)
    return RemoveIndentedEmptyLines(template.substitute(values))

  def GetTraceEventForNameTemplate(self, name_template, values):
    name = Template(name_template).substitute(values)
    return '  TRACE_EVENT0("jni", "%s");\n' % name


def WrapOutput(output):
  ret = []
  for line in output.splitlines():
    # Do not wrap preprocessor directives or comments.
    if len(line) < _WRAP_LINE_LENGTH or line[0] == '#' or line.startswith('//'):
      ret.append(line)
    else:
      # Assumes that the line is not already indented as a continuation line,
      # which is not always true (oh well).
      first_line_indent = (len(line) - len(line.lstrip()))
      wrapper = _WRAPPERS_BY_INDENT[first_line_indent]
      ret.extend(wrapper.wrap(line))
  ret += ['']
  return '\n'.join(ret)


def GetScriptName():
  script_components = os.path.abspath(__file__).split(os.path.sep)
  base_index = 0
  for idx, value in enumerate(script_components):
    if value == 'base' or value == 'third_party':
      base_index = idx
      break
  return os.sep.join(script_components[base_index:])


def _RemoveStaleHeaders(path, output_names):
  if not os.path.isdir(path):
    return
  # Do not remove output files so that timestamps on declared outputs are not
  # modified unless their contents are changed (avoids reverse deps needing to
  # be rebuilt).
  preserve = set(output_names)
  for root, _, files in os.walk(path):
    for f in files:
      if f not in preserve:
        file_path = os.path.join(root, f)
        if os.path.isfile(file_path) and file_path.endswith('.h'):
          os.remove(file_path)


def _CheckSameModule(jni_objs):
  files_by_module = collections.defaultdict(list)
  for jni_obj in jni_objs:
    if jni_obj.proxy_natives:
      files_by_module[jni_obj.module_name].append(jni_obj.filename)
  if len(files_by_module) > 1:
    sys.stderr.write(
        'Multiple values for @NativeMethods(moduleName) is not supported.\n')
    for module_name, filenames in files_by_module.items():
      sys.stderr.write(f'module_name={module_name}\n')
      for filename in filenames:
        sys.stderr.write(f'  {filename}\n')
    sys.exit(1)


def _CheckNotEmpty(jni_objs):
  has_empty = False
  for jni_obj in jni_objs:
    if not (jni_obj.natives or jni_obj.called_by_natives):
      has_empty = True
      sys.stderr.write(f'No native methods found in {jni_obj.filename}.\n')
  if has_empty:
    sys.exit(1)


def _ParseClassFiles(jar_file, class_files, options):
  # Parse javap output.
  ret = []
  with tempfile.TemporaryDirectory() as temp_dir:
    with zipfile.ZipFile(jar_file) as z:
      z.extractall(temp_dir, class_files)
    for class_file in class_files:
      class_file = os.path.join(temp_dir, class_file)
      ret.append(JNIFromJavaP.CreateFromClass(class_file, options))
  return ret


def _CreateSrcJar(srcjar_path, gen_jni_class, jni_objs, *, script_name):
  with action_helpers.atomic_output(srcjar_path) as f:
    with zipfile.ZipFile(f, 'w') as srcjar:
      for jni_obj in jni_objs:
        if not jni_obj.proxy_natives:
          continue
        content = proxy_impl_java.Generate(jni_obj,
                                           gen_jni_class=gen_jni_class,
                                           script_name=script_name)
        zip_path = f'{jni_obj.java_class.full_name_with_slashes}Jni.java'
        zip_helpers.add_to_zip_hermetic(srcjar, zip_path, data=content)

      content = placeholder_gen_jni_java.Generate(jni_objs,
                                                  gen_jni_class=gen_jni_class,
                                                  script_name=script_name)
      zip_path = f'{gen_jni_class.full_name_with_slashes}.java'
      zip_helpers.add_to_zip_hermetic(srcjar, zip_path, data=content)


def DoGeneration(options):
  try:
    if options.jar_file:
      jni_objs = _ParseClassFiles(options.jar_file, options.input_files,
                                  options)
    else:
      jni_objs = [
          JNIFromJavaSource.CreateFromFile(f, options)
          for f in options.input_files
      ]
      _CheckNotEmpty(jni_objs)
      _CheckSameModule(jni_objs)
  except (ParseError, SyntaxError) as e:
    sys.stderr.write(f'{e}\n')
    sys.exit(1)

  # Write .h files
  for jni_obj, header_name in zip(jni_objs, options.output_names):
    output_file = os.path.join(options.output_dir, header_name)
    content = jni_obj.GetContent()
    with action_helpers.atomic_output(output_file, 'w') as f:
      f.write(content)

  # Write .srcjar
  if options.srcjar_path:
    # module_name is set only for proxy_natives.
    jni_objs = [x for x in jni_objs if x.proxy_natives]
    if jni_objs:
      gen_jni_class = proxy.get_gen_jni_class(
          short=False,
          name_prefix=jni_objs[0].module_name,
          package_prefix=options.package_prefix)
      _CreateSrcJar(options.srcjar_path,
                    gen_jni_class,
                    jni_objs,
                    script_name=GetScriptName())
    else:
      # Only @CalledByNatives.
      zipfile.ZipFile(options.srcjar_path, 'w').close()


def main(parser, args):
  if args.jar_file and args.package_prefix:
    parser.error('--package-prefix not implemented for --jar-file')

  if args.jar_file and not args.javap:
    args.javap = shutil.which('javap')
    if not args.javap:
      parser.error('Could not find "javap" on your PATH. Use --javap to '
                   'specify its location.')

  # Kotlin files are not supported by jni_generator.py, but they do end up in
  # the list of source files passed to jni_generator.py.
  input_files = [f for f in args.input_files if not f.endswith('.kt')]

  # Remove existing headers so that moving .java source files but not updating
  # the corresponding C++ include will be a compile failure (otherwise
  # incremental builds will usually not catch this).
  _RemoveStaleHeaders(args.output_dir, args.output_names)

  DoGeneration(args)
