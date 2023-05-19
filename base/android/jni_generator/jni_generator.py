#!/usr/bin/env python3
# Copyright 2012 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Extracts native methods from a Java file and generates the JNI bindings.
If you change this, please run and update the tests."""
import argparse
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

# TODO(crbug.com/1410871): Move nested classes into separate files and remove
#     this hack (or create a separate main.py).
# https://stackoverflow.com/questions/15159854/python-namespace-main-class-not-isinstance-of-package-class
if __name__ == '__main__':
  import jni_generator
  sys.exit(jni_generator.main())

_FILE_DIR = os.path.dirname(__file__)
_CHROMIUM_SRC = os.path.join(_FILE_DIR, os.pardir, os.pardir, os.pardir)
_BUILD_ANDROID_GYP = os.path.join(_CHROMIUM_SRC, 'build', 'android', 'gyp')

# Item 0 of sys.path is the directory of the main file; item 1 is PYTHONPATH
# (if set); item 2 is system libraries.
sys.path.insert(1, _BUILD_ANDROID_GYP)

from util import build_utils
import action_helpers  # build_utils adds //build to sys.path.

import common
import models
import parse
import proxy
import type_resolver


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

JAVA_POD_TYPE_MAP = {
    'int': 'jint',
    'byte': 'jbyte',
    'char': 'jchar',
    'short': 'jshort',
    'boolean': 'jboolean',
    'long': 'jlong',
    'double': 'jdouble',
    'float': 'jfloat',
}

JAVA_TYPE_MAP = {
    'void': 'void',
    'String': 'jstring',
    'Class': 'jclass',
    'Throwable': 'jthrowable',
    'java/lang/String': 'jstring',
    'java/lang/Class': 'jclass',
    'java/lang/Throwable': 'jthrowable',
}


class ParseError(SyntaxError):
  """Exception thrown when we can't parse the input file."""

  def __init__(self, description, *context_lines):
    super().__init__()
    self.description = description
    self.context_lines = context_lines

  def __str__(self):
    context = '\n'.join(self.context_lines)
    return '***\nERROR: %s\n\n%s\n***' % (self.description, context)


class NativeMethod(object):
  """Describes a C/C++ method that is called by Java code"""

  def __init__(self, **kwargs):
    self.return_type = kwargs['return_type']
    self.params = kwargs['params']
    self.is_proxy = kwargs.get('is_proxy', False)
    self.static = kwargs.get('static', self.is_proxy)

    self.name = kwargs['name']
    # Proxy methods don't have a native prefix so the first letter is
    # lowercase. But we still want the CPP declaration to use upper camel
    # case for the method name.
    self.cpp_name = self.name[0].upper() + self.name[1:]
    self.is_test_only = _NameIsTestOnly(self.name)

    if self.params:
      assert type(self.params) is list
      assert type(self.params[0]) is models.Param

    if self.is_proxy:
      self.proxy_params = [
          dataclasses.replace(p, datatype=JavaTypeToProxyCast(p.datatype))
          for p in self.params
      ]
      self.proxy_return_type = JavaTypeToProxyCast(self.return_type)
      self.proxy_return_and_signature = (self.proxy_return_type,
                                         tuple(p.datatype
                                               for p in self.proxy_params))

      self.proxy_name, self.hashed_proxy_name = proxy.create_method_names(
          kwargs['java_class'], self.name, self.is_test_only)
      self.switch_num = None
    else:
      self.proxy_params = self.params
      self.proxy_return_type = self.return_type

    ptr_type = kwargs.get('ptr_type', 'int')
    if (self.params and self.params[0].datatype == ptr_type
        and self.params[0].name.startswith('native')):
      self.ptr_type = ptr_type
      self.type = 'method'
      self.p0_type = kwargs.get('p0_type')
      if self.p0_type is None:
        self.p0_type = self.params[0].name[len('native'):]
        if kwargs.get('native_class_name'):
          self.p0_type = kwargs['native_class_name']
    else:
      self.type = 'function'
    self.method_id_var_name = kwargs.get('method_id_var_name', None)


class CalledByNative(object):
  """Describes a java method exported to c/c++"""

  def __init__(self, **kwargs):
    self.system_class = kwargs['system_class']
    self.unchecked = kwargs['unchecked']
    self.static = kwargs['static']
    self.java_class_name = kwargs['java_class_name']
    self.return_type = kwargs['return_type']
    self.name = kwargs['name']
    self.params = kwargs['params']
    self.method_id_var_name = kwargs.get('method_id_var_name', None)
    self.signature = kwargs.get('signature')
    self.is_constructor = kwargs.get('is_constructor', False)
    self.env_call = GetEnvCall(self.is_constructor, self.static,
                               self.return_type)
    self.static_cast = GetStaticCastForReturnType(self.return_type)


class ConstantField(object):

  def __init__(self, **kwargs):
    self.name = kwargs['name']
    self.value = kwargs['value']


def JavaDataTypeToC(java_type):
  """Returns a C datatype for the given java type."""
  java_type = parse.strip_generics(java_type)
  if java_type in JAVA_POD_TYPE_MAP:
    return JAVA_POD_TYPE_MAP[java_type]
  elif java_type in JAVA_TYPE_MAP:
    return JAVA_TYPE_MAP[java_type]
  elif java_type.endswith('[]'):
    if java_type[:-2] in JAVA_POD_TYPE_MAP:
      return JAVA_POD_TYPE_MAP[java_type[:-2]] + 'Array'
    return 'jobjectArray'
  else:
    return 'jobject'


def JavaTypeToProxyCast(java_type):
  """Maps from a java type to the type used by the native proxy GEN_JNI class"""
  # All the types and array types of JAVA_TYPE_MAP become jobjectArray across
  # jni but they still need to be passed as the original type on the java side.
  raw_type = java_type.rstrip('[]')
  if raw_type in JAVA_POD_TYPE_MAP or raw_type in JAVA_TYPE_MAP:
    return java_type

  # All other types should just be passed as Objects or Object arrays.
  return 'Object' + java_type[len(raw_type):]


def WrapCTypeForDeclaration(c_type):
  """Wrap the C datatype in a JavaRef if required."""
  if re.match(RE_SCOPED_JNI_TYPES, c_type):
    return 'const base::android::JavaParamRef<' + c_type + '>&'
  else:
    return c_type


def _JavaDataTypeToCForDeclaration(java_type):
  """Returns a JavaRef-wrapped C datatype for the given java type."""
  return WrapCTypeForDeclaration(JavaDataTypeToC(java_type))


def JavaDataTypeToCForCalledByNativeParam(java_type):
  """Returns a C datatype to be when calling from native."""
  if java_type == 'int':
    return 'JniIntWrapper'
  else:
    c_type = JavaDataTypeToC(java_type)
    if re.match(RE_SCOPED_JNI_TYPES, c_type):
      return 'const base::android::JavaRef<' + c_type + '>&'
    else:
      return c_type


def JavaReturnValueToC(java_type):
  """Returns a valid C return value for the given java type."""
  java_pod_type_map = {
      'int': '0',
      'byte': '0',
      'char': '0',
      'short': '0',
      'boolean': 'false',
      'long': '0',
      'double': '0',
      'float': '0',
      'void': ''
  }
  return java_pod_type_map.get(java_type, 'NULL')


def _GetJNIFirstParam(native, for_declaration):
  c_type = 'jclass' if native.static else 'jobject'

  if for_declaration:
    c_type = WrapCTypeForDeclaration(c_type)
  return [c_type + ' jcaller']


def _GetParamsInDeclaration(native):
  """Returns the params for the forward declaration.

  Args:
    native: the native dictionary describing the method.

  Returns:
    A string containing the params.
  """
  if not native.static:
    return _GetJNIFirstParam(native, True) + [
        _JavaDataTypeToCForDeclaration(param.datatype) + ' ' + param.name
        for param in native.params
    ]
  return [
      _JavaDataTypeToCForDeclaration(param.datatype) + ' ' + param.name
      for param in native.params
  ]


def GetParamsInStub(native):
  """Returns the params for the stub declaration.

  Args:
    native: the native dictionary describing the method.

  Returns:
    A string containing the params.
  """
  params = [JavaDataTypeToC(p.datatype) + ' ' + p.name for p in native.params]
  params = _GetJNIFirstParam(native, False) + params
  return ',\n    '.join(params)


def _NameIsTestOnly(name):
  return name.endswith('ForTest') or name.endswith('ForTesting')


def ExtractNatives(contents, ptr_type):
  """Returns a list of dict containing information about a native method."""
  natives = []
  for match in _EXTRACT_NATIVES_REGEX.finditer(contents):
    native = NativeMethod(static='static' in match.group('qualifiers'),
                          native_class_name=match.group('native_class_name'),
                          return_type=match.group('return_type'),
                          name=match.group('name').replace('native', ''),
                          params=parse.parse_param_list(match.group('params')),
                          ptr_type=ptr_type)
    natives += [native]
  return natives


def GetRegistrationFunctionName(fully_qualified_class):
  """Returns the register name with a given class."""
  return 'RegisterNative_' + common.EscapeClassName(fully_qualified_class)


def GetStaticCastForReturnType(return_type):
  type_map = {
      'String': 'jstring',
      'java/lang/String': 'jstring',
      'Class': 'jclass',
      'java/lang/Class': 'jclass',
      'Throwable': 'jthrowable',
      'java/lang/Throwable': 'jthrowable',
      'boolean[]': 'jbooleanArray',
      'byte[]': 'jbyteArray',
      'char[]': 'jcharArray',
      'short[]': 'jshortArray',
      'int[]': 'jintArray',
      'long[]': 'jlongArray',
      'float[]': 'jfloatArray',
      'double[]': 'jdoubleArray'
  }
  return_type = parse.strip_generics(return_type)
  ret = type_map.get(return_type, None)
  if ret:
    return ret
  if return_type.endswith('[]'):
    return 'jobjectArray'
  return None


def GetEnvCall(is_constructor, is_static, return_type):
  """Maps the types availabe via env->Call__Method."""
  if is_constructor:
    return 'NewObject'
  env_call_map = {
      'boolean': 'Boolean',
      'byte': 'Byte',
      'char': 'Char',
      'short': 'Short',
      'int': 'Int',
      'long': 'Long',
      'float': 'Float',
      'void': 'Void',
      'double': 'Double',
      'Object': 'Object',
  }
  call = env_call_map.get(return_type, 'Object')
  if is_static:
    call = 'Static' + call
  return 'Call' + call + 'Method'


def GetMangledParam(datatype):
  """Returns a mangled identifier for the datatype."""
  if len(datatype) <= 2:
    return datatype.replace('[', 'A')
  ret = ''
  for i in range(1, len(datatype)):
    c = datatype[i]
    if c == '[':
      ret += 'A'
    elif c.isupper() or datatype[i - 1] in ['/', 'L']:
      ret += c.upper()
  return ret


def GetMangledMethodName(resolver, name, params, return_type):
  """Returns a mangled method name for the given signature.

     The returned name can be used as a C identifier and will be unique for all
     valid overloads of the same method.

  Returns:
      A mangled name.
  """
  mangled_items = []
  for datatype in [return_type] + [x.datatype for x in params]:
    mangled_items += [GetMangledParam(resolver.java_to_jni(datatype))]
  mangled_name = name + '_'.join(mangled_items)
  assert re.match(r'[0-9a-zA-Z_]+', mangled_name)
  return mangled_name


def MangleCalledByNatives(resolver, called_by_natives):
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
      method_id_var_name = GetMangledMethodName(resolver, method_name,
                                                called_by_native.params,
                                                called_by_native.return_type)
    called_by_native.method_id_var_name = method_id_var_name
  return called_by_natives


# Regex to match the JNI types that should be wrapped in a JavaRef.
RE_SCOPED_JNI_TYPES = re.compile('jobject|jclass|jstring|jthrowable|.*Array')

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


def ExtractCalledByNatives(resolver, contents):
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

    called_by_natives += [
        CalledByNative(system_class=False,
                       unchecked='Unchecked' in match.group('Unchecked'),
                       static='static' in match.group('prefix'),
                       java_class_name=match.group('annotation') or '',
                       return_type=return_type,
                       name=name,
                       is_constructor=is_constructor,
                       params=parse.parse_param_list(match.group('params')))
    ]
  # Check for any @CalledByNative occurrences that weren't matched.
  unmatched_lines = re.sub(RE_CALLED_BY_NATIVE, '', contents).split('\n')
  for line1, line2 in zip(unmatched_lines, unmatched_lines[1:]):
    if '@CalledByNative' in line1:
      raise ParseError('could not parse @CalledByNative method signature',
                       line1, line2)
  return MangleCalledByNatives(resolver, called_by_natives)


class JNIFromJavaP(object):
  """Uses 'javap' to parse a .class file and generate the JNI header file."""

  def __init__(self, contents, options):
    self.options = options
    for line in contents:
      m = re.match('.*?(public).*?(?:class|interface) (\S+?)( |\Z)', line)
      if m:
        fqn = m.group(2).split('<', 1)[0].replace('.', '/')
        self.java_class = models.JavaClass(fqn, visibility=m.group(1))
        break
    else:
      raise SyntaxError('Could not find java class in javap output')
    self.jni_namespace = options.namespace or 'JNI_' + self.java_class.name
    self.type_resolver = type_resolver.TypeResolver(self.java_class)
    re_method = re.compile('(?P<prefix>.*?)(?P<return_type>\S+?) (?P<name>\w+?)'
                           '\((?P<params>.*?)\)')
    self.called_by_natives = []
    for lineno, content in enumerate(contents[2:], 2):
      match = re.match(re_method, content)
      if not match:
        continue
      self.called_by_natives += [
          CalledByNative(
              system_class=True,
              unchecked=options.unchecked_exceptions,
              static='static' in match.group('prefix'),
              java_class_name='',
              return_type=match.group('return_type').replace('.', '/'),
              name=match.group('name'),
              params=parse.parse_param_list(match.group('params'),
                                            from_javap=True),
              signature=parse.parse_javap_signature(contents[lineno + 1]))
      ]
    re_constructor = re.compile('(.*?)public ' +
                                self.java_class.full_name_with_dots +
                                '\((?P<params>.*?)\)')
    for lineno, content in enumerate(contents[2:], 2):
      match = re.match(re_constructor, content)
      if not match:
        continue
      self.called_by_natives += [
          CalledByNative(system_class=True,
                         unchecked=options.unchecked_exceptions,
                         static=False,
                         java_class_name='',
                         return_type=self.java_class.full_name_with_slashes,
                         name='Constructor',
                         params=parse.parse_param_list(match.group('params'),
                                                       from_javap=True),
                         signature=parse.parse_javap_signature(contents[lineno +
                                                                        1]),
                         is_constructor=True)
      ]
    self.called_by_natives = MangleCalledByNatives(self.type_resolver,
                                                   self.called_by_natives)
    self.constant_fields = []
    re_constant_field = re.compile('.*?public static final int (?P<name>.*?);')
    re_constant_field_value = re.compile(
        '.*?Constant(Value| value): int (?P<value>(-*[0-9]+)?)')
    for lineno, content in enumerate(contents[2:], 2):
      match = re.match(re_constant_field, content)
      if not match:
        continue
      value = re.match(re_constant_field_value, contents[lineno + 2])
      if not value:
        value = re.match(re_constant_field_value, contents[lineno + 3])
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
    jni_from_javap = JNIFromJavaP(stdout.split('\n'), options)
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

    proxy_natives = []
    for parsed_method in parsed_file.proxy_methods:
      proxy_natives.append(
          NativeMethod(is_proxy=True,
                       java_class=self.java_class,
                       name=parsed_method.name,
                       return_type=parsed_method.return_type,
                       params=parsed_method.params,
                       native_class_name=parsed_method.native_class_name,
                       ptr_type='long'))

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
        method_name = common.EscapeClassName(native.hashed_proxy_name)
      else:
        method_name = common.EscapeClassName(native.proxy_name)
      return 'Java_%s_%s' % (common.EscapeClassName(
          self.gen_jni_class.full_name_with_slashes), method_name)

    template = Template('Java_${JAVA_NAME}_native${NAME}')

    java_name = self.fully_qualified_class

    values = {
        'NAME': native.cpp_name,
        'JAVA_NAME': common.EscapeClassName(java_name)
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
          'JAVA_CLASS': common.EscapeClassName(full_clazz),
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
          common.EscapeClassName(full_clazz),
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
               called_by_natives, constant_fields, resolver, options):
    self.namespace = namespace
    self.java_class = java_class
    self.class_name = java_class.name
    self.natives = natives
    self.called_by_natives = called_by_natives
    self.header_guard = java_class.full_name_with_slashes.replace('/',
                                                                  '_') + '_JNI'
    self.constant_fields = constant_fields
    self.type_resolver = resolver
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
    if not self.options.includes:
      return ''
    includes = self.options.includes.split(',')
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
        JavaDataTypeToCForCalledByNativeParam(param.datatype) + ' ' + param.name
        for param in called_by_native.params
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
      c_type = JavaDataTypeToC(p.datatype)
      if re.match(RE_SCOPED_JNI_TYPES, c_type):
        params_in_call.append(self.GetJavaParamRefForCall(c_type, p.name))
      else:
        params_in_call.append(p.name)

    params_in_declaration = _GetParamsInDeclaration(native)
    params_in_call = ', '.join(params_in_call)

    return_type = return_declaration = JavaDataTypeToC(native.return_type)
    post_call = ''
    if re.match(RE_SCOPED_JNI_TYPES, return_type):
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
      optional_error_return = JavaReturnValueToC(native.return_type)
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
    if param.datatype == 'int':
      return 'as_jint(' + param.name + ')'
    elif re.match(RE_SCOPED_JNI_TYPES, JavaDataTypeToC(param.datatype)):
      return param.name + '.obj()'
    else:
      return param.name

  def GetArgumentsInCall(self, params):
    """Return a string of arguments to call from native into Java"""
    return [self.GetArgument(p) for p in params]

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
    params_in_call = ', '.join(self.GetArgumentsInCall(called_by_native.params))
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
    return_type = JavaDataTypeToC(called_by_native.return_type)
    optional_error_return = JavaReturnValueToC(called_by_native.return_type)
    if optional_error_return:
      optional_error_return = ', ' + optional_error_return
    return_declaration = ''
    return_clause = ''
    if return_type != 'void':
      pre_call = ' ' + pre_call
      return_declaration = return_type + ' ret ='
      if re.match(RE_SCOPED_JNI_TYPES, return_type):
        return_type = 'base::android::ScopedJavaLocalRef<' + return_type + '>'
        return_clause = 'return ' + return_type + '(env, ret);'
      else:
        return_clause = 'return ret;'
    profiling_leaving_native = ''
    if self.options.enable_profiling:
      profiling_leaving_native = '  JNI_SAVE_FRAME_POINTER;\n'
    jni_name = called_by_native.name
    jni_return_type = called_by_native.return_type
    if called_by_native.is_constructor:
      jni_name = '<init>'
      jni_return_type = 'void'
    if called_by_native.signature:
      jni_signature = called_by_native.signature
    else:
      jni_signature = self.type_resolver.create_signature(
          called_by_native.params, jni_return_type)
    java_name_full = java_class.replace('/', '.') + '.' + jni_name
    return {
        'JAVA_CLASS_ONLY': java_class_only,
        'JAVA_CLASS': common.EscapeClassName(java_class),
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
        'JNI_SIGNATURE': jni_signature,
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
          ${JNI_SIGNATURE},
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


def main():
  description = """
This script will parse the given java source code extracting the native
declarations and print the header file to stdout (or a file).
See SampleForTests.java for more details.
  """
  parser = argparse.ArgumentParser(description=description)

  parser.add_argument(
      '-j',
      '--jar_file',
      dest='jar_file',
      help='Extract the list of input files from'
      ' a specified jar file.'
      ' Uses javap to extract the methods from a'
      ' pre-compiled class. --input should point'
      ' to pre-compiled Java .class files.')
  parser.add_argument(
      '-n',
      dest='namespace',
      help='Uses as a namespace in the generated header '
      'instead of the javap class name, or when there is '
      'no JNINamespace annotation in the java source.')
  parser.add_argument('--input_file',
                      action='append',
                      required=True,
                      dest='input_files',
                      help='Input filenames, or paths within a .jar if '
                      '--jar-file is used.')
  parser.add_argument('--output_dir', required=True, help='Output directory.')
  # TODO(agrieve): --prev_output_dir used only to make incremental builds work.
  #     Remove --prev_output_dir at some point after 2022.
  parser.add_argument('--prev_output_dir',
                      help='Delete headers found in this directory.')
  parser.add_argument('--output_name',
                      action='append',
                      dest='output_names',
                      help='Output filenames within output directory.')
  parser.add_argument(
      '--includes',
      help='The comma-separated list of header files to '
      'include in the generated header.')
  parser.add_argument(
      '--ptr_type',
      default='int',
      choices=['int', 'long'],
      help='The type used to represent native pointers in '
      'Java code. For 32-bit, use int; '
      'for 64-bit, use long.')
  parser.add_argument('--cpp', default='cpp', help='The path to cpp command.')
  parser.add_argument(
      '--javap',
      default=build_utils.JAVAP_PATH,
      help='The path to javap command.')
  parser.add_argument(
      '--enable_profiling',
      action='store_true',
      help='Add additional profiling instrumentation.')
  parser.add_argument('--unchecked_exceptions',
                      action='store_true',
                      help='Do not check that no exceptions were thrown.')
  parser.add_argument(
      '--use_proxy_hash',
      action='store_true',
      help='Hashes the native declaration of methods used '
      'in @JniNatives interface.')
  parser.add_argument('--enable_jni_multiplexing',
                      action='store_true',
                      help='Enables JNI multiplexing for Java native methods')
  parser.add_argument(
      '--split_name',
      help='Split name that the Java classes should be loaded from.')
  parser.add_argument(
      '--package_prefix',
      help='Adds a prefix to the classes fully qualified-name. Effectively '
      'changing a class name fromfoo.bar -> prefix.foo.bar')
  # TODO(agrieve): --stamp used only to make incremental builds work.
  #     Remove --stamp at some point after 2022.
  parser.add_argument('--stamp',
                      help='Process --prev_output_dir and touch this file.')
  args = parser.parse_args()
  if args.jar_file and args.package_prefix:
    parser.error('--package_prefix not implemented for --jar_file')

  # Kotlin files are not supported by jni_generator.py, but they do end up in
  # the list of source files passed to jni_generator.py.
  input_files = [f for f in args.input_files if not f.endswith('.kt')]

  if args.prev_output_dir:
    _RemoveStaleHeaders(args.prev_output_dir, [])

  if args.stamp:
    build_utils.Touch(args.stamp)
    sys.exit(0)

  # Remove existing headers so that moving .java source files but not updating
  # the corresponding C++ include will be a compile failure (otherwise
  # incremental builds will usually not catch this).
  _RemoveStaleHeaders(args.output_dir, args.output_names)

  DoGeneration(args)


if __name__ == '__main__':
  sys.exit(main())
