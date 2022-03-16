#!/usr/bin/env python
# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Generates GEN_JNI.java (or N.java) and helper for manual JNI registration.

Creates a header file with two static functions: RegisterMainDexNatives() and
RegisterNonMainDexNatives(). Together, these will use manual JNI registration
to register all native methods that exist within an application."""

import argparse
import collections
import functools
import multiprocessing
import os
import string
import sys
import zipfile

import jni_generator
from util import build_utils

# All but FULL_CLASS_NAME, which is used only for sorting.
MERGEABLE_KEYS = [
    'CLASS_PATH_DECLARATIONS',
    'FORWARD_DECLARATIONS',
    'JNI_NATIVE_METHOD',
    'JNI_NATIVE_METHOD_ARRAY',
    'PROXY_NATIVE_SIGNATURES',
    'FORWARDING_PROXY_METHODS',
    'PROXY_NATIVE_METHOD_ARRAY',
    'PROXY_NATIVE_METHOD_ARRAY_MAIN_DEX',
    'REGISTER_MAIN_DEX_NATIVES',
    'REGISTER_NON_MAIN_DEX_NATIVES',
]


def _Generate(java_file_paths,
              srcjar_path,
              proxy_opts,
              header_path=None,
              namespace=''):
  """Generates files required to perform JNI registration.

  Generates a srcjar containing a single class, GEN_JNI, that contains all
  native method declarations.

  Optionally generates a header file that provides functions
  (RegisterMainDexNatives and RegisterNonMainDexNatives) to perform
  JNI registration.

  Args:
    java_file_paths: A list of java file paths.
    srcjar_path: Path to the GEN_JNI srcjar.
    header_path: If specified, generates a header file in this location.
    namespace: If specified, sets the namespace for the generated header file.
  """
  # For JNI multiplexing, a 16-bit prefix is used to identify each individual
  # java file path. This allows fewer multiplexed functions to resolve multiple
  # different native functions with the same signature across the JNI boundary
  # using switch statements. Should not exceed 65536 (2**16) number of paths.
  assert len(java_file_paths) < 65536
  java_path_prefix_tuples = [(path, index)
                             for index, path in enumerate(java_file_paths)]
  # Without multiprocessing, script takes ~13 seconds for chrome_public_apk
  # on a z620. With multiprocessing, takes ~2 seconds.
  results = []
  with multiprocessing.Pool() as pool:
    for d in pool.imap_unordered(
        functools.partial(
            _DictForPathAndPrefix,
            use_proxy_hash=proxy_opts.use_hash,
            enable_jni_multiplexing=proxy_opts.enable_jni_multiplexing),
        java_path_prefix_tuples):
      if d:
        results.append(d)

  # Sort to make output deterministic.
  results.sort(key=lambda d: d['FULL_CLASS_NAME'])

  combined_dict = {}
  for key in MERGEABLE_KEYS:
    combined_dict[key] = ''.join(d.get(key, '') for d in results)
  # PROXY_NATIVE_SIGNATURES will have duplicates for JNI multiplexing since
  # all native methods with similar signatures map to the same proxy.
  if proxy_opts.enable_jni_multiplexing:
    proxy_signatures_list = sorted(
        set(combined_dict['PROXY_NATIVE_SIGNATURES'].split('\n')))
    combined_dict['PROXY_NATIVE_SIGNATURES'] = '\n'.join(
        signature for signature in proxy_signatures_list)

  if header_path:
    combined_dict['HEADER_GUARD'] = \
        os.path.splitext(header_path)[0].replace('/', '_').upper() + '_'
    combined_dict['NAMESPACE'] = namespace
    header_content = CreateFromDict(combined_dict, proxy_opts.use_hash)
    with build_utils.AtomicOutput(header_path, mode='w') as f:
      f.write(header_content)

  with build_utils.AtomicOutput(srcjar_path) as f:
    with zipfile.ZipFile(f, 'w') as srcjar:
      if proxy_opts.use_hash:
        # J/N.java
        build_utils.AddToZipHermetic(
            srcjar,
            '%s.java' % jni_generator.ProxyHelpers.GetQualifiedClass(True),
            data=CreateProxyJavaFromDict(combined_dict, proxy_opts))
        # org/chromium/base/natives/GEN_JNI.java
        build_utils.AddToZipHermetic(
            srcjar,
            '%s.java' % jni_generator.ProxyHelpers.GetQualifiedClass(False),
            data=CreateProxyJavaFromDict(
                combined_dict, proxy_opts, forwarding=True))
      else:
        # org/chromium/base/natives/GEN_JNI.java
        build_utils.AddToZipHermetic(
            srcjar,
            '%s.java' % jni_generator.ProxyHelpers.GetQualifiedClass(False),
            data=CreateProxyJavaFromDict(combined_dict, proxy_opts))


# A wrapper for imap_ordered to call with a tuple.
def _DictForPathAndPrefix(path_prefix_tuple, use_proxy_hash,
                          enable_jni_multiplexing):
  path, switch_prefix = path_prefix_tuple
  return _DictForPath(path,
                      use_proxy_hash=use_proxy_hash,
                      enable_jni_multiplexing=enable_jni_multiplexing,
                      switch_prefix=switch_prefix)


def _DictForPath(path,
                 use_proxy_hash=False,
                 enable_jni_multiplexing=False,
                 switch_prefix=None):
  with open(path) as f:
    contents = jni_generator.RemoveComments(f.read())
    if '@JniIgnoreNatives' in contents:
      return None

  fully_qualified_class = jni_generator.ExtractFullyQualifiedJavaClassName(
      path, contents)
  natives = jni_generator.ExtractNatives(contents, 'long')

  natives += jni_generator.ProxyHelpers.ExtractStaticProxyNatives(
      fully_qualified_class=fully_qualified_class,
      contents=contents,
      ptr_type='long')
  if len(natives) == 0:
    return None
  namespace = jni_generator.ExtractJNINamespace(contents)
  jni_params = jni_generator.JniParams(fully_qualified_class)
  jni_params.ExtractImportsAndInnerClasses(contents)
  is_main_dex = jni_generator.IsMainDexJavaClass(contents)
  header_generator = HeaderGenerator(
      namespace,
      fully_qualified_class,
      natives,
      jni_params,
      is_main_dex,
      use_proxy_hash,
      enable_jni_multiplexing=enable_jni_multiplexing,
      switch_prefix=switch_prefix)
  return header_generator.Generate()


def _SetProxyRegistrationFields(registration_dict, use_hash):
  registration_template = string.Template("""\

static const JNINativeMethod kMethods_${ESCAPED_PROXY_CLASS}[] = {
${KMETHODS}
};

namespace {

JNI_REGISTRATION_EXPORT bool ${REGISTRATION_NAME}(JNIEnv* env) {
  const int number_of_methods = std::size(kMethods_${ESCAPED_PROXY_CLASS});

  base::android::ScopedJavaLocalRef<jclass> native_clazz =
      base::android::GetClass(env, "${PROXY_CLASS}");
  if (env->RegisterNatives(
      native_clazz.obj(),
      kMethods_${ESCAPED_PROXY_CLASS},
      number_of_methods) < 0) {

    jni_generator::HandleRegistrationError(env, native_clazz.obj(), __FILE__);
    return false;
  }

  return true;
}

}  // namespace
""")

  registration_call = string.Template("""\

  // Register natives in a proxy.
  if (!${REGISTRATION_NAME}(env)) {
    return false;
  }
""")

  sub_dict = {
      'ESCAPED_PROXY_CLASS':
      jni_generator.EscapeClassName(
          jni_generator.ProxyHelpers.GetQualifiedClass(use_hash)),
      'PROXY_CLASS':
      jni_generator.ProxyHelpers.GetQualifiedClass(use_hash),
      'KMETHODS':
      registration_dict['PROXY_NATIVE_METHOD_ARRAY'],
      'REGISTRATION_NAME':
      jni_generator.GetRegistrationFunctionName(
          jni_generator.ProxyHelpers.GetQualifiedClass(use_hash)),
  }

  if registration_dict['PROXY_NATIVE_METHOD_ARRAY']:
    proxy_native_array = registration_template.substitute(sub_dict)
    proxy_natives_registration = registration_call.substitute(sub_dict)
  else:
    proxy_native_array = ''
    proxy_natives_registration = ''

  if registration_dict['PROXY_NATIVE_METHOD_ARRAY_MAIN_DEX']:
    sub_dict['REGISTRATION_NAME'] += 'MAIN_DEX'
    sub_dict['ESCAPED_PROXY_CLASS'] += 'MAIN_DEX'
    sub_dict['KMETHODS'] = (
        registration_dict['PROXY_NATIVE_METHOD_ARRAY_MAIN_DEX'])
    proxy_native_array += registration_template.substitute(sub_dict)
    main_dex_call = registration_call.substitute(sub_dict)
  else:
    main_dex_call = ''

  registration_dict['PROXY_NATIVE_METHOD_ARRAY'] = proxy_native_array
  registration_dict['REGISTER_PROXY_NATIVES'] = proxy_natives_registration
  registration_dict['REGISTER_MAIN_DEX_PROXY_NATIVES'] = main_dex_call


def CreateProxyJavaFromDict(registration_dict, proxy_opts, forwarding=False):
  template = string.Template("""\
// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package ${PACKAGE};

// This file is autogenerated by
//     base/android/jni_generator/jni_registration_generator.py
// Please do not change its content.

public class ${CLASS_NAME} {
${FIELDS}
${METHODS}
}
""")

  is_natives_class = not forwarding and proxy_opts.use_hash
  class_name = jni_generator.ProxyHelpers.GetClass(is_natives_class)
  package = jni_generator.ProxyHelpers.GetPackage(is_natives_class)

  if forwarding or not proxy_opts.use_hash:
    fields = string.Template("""\
    public static final boolean TESTING_ENABLED = ${TESTING_ENABLED};
    public static final boolean REQUIRE_MOCK = ${REQUIRE_MOCK};
""").substitute({
        'TESTING_ENABLED': str(proxy_opts.enable_mocks).lower(),
        'REQUIRE_MOCK': str(proxy_opts.require_mocks).lower(),
    })
  else:
    fields = ''

  if forwarding:
    methods = registration_dict['FORWARDING_PROXY_METHODS']
  else:
    methods = registration_dict['PROXY_NATIVE_SIGNATURES']

  return template.substitute({
      'CLASS_NAME': class_name,
      'FIELDS': fields,
      'PACKAGE': package.replace('/', '.'),
      'METHODS': methods
  })


def CreateFromDict(registration_dict, use_hash):
  """Returns the content of the header file."""

  template = string.Template("""\
// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


// This file is autogenerated by
//     base/android/jni_generator/jni_registration_generator.py
// Please do not change its content.

#ifndef ${HEADER_GUARD}
#define ${HEADER_GUARD}

#include <jni.h>

#include <iterator>

#include "base/android/jni_generator/jni_generator_helper.h"
#include "base/android/jni_int_wrapper.h"


// Step 1: Forward declarations (classes).
${CLASS_PATH_DECLARATIONS}

// Step 2: Forward declarations (methods).

${FORWARD_DECLARATIONS}

// Step 3: Method declarations.

${JNI_NATIVE_METHOD_ARRAY}\
${PROXY_NATIVE_METHOD_ARRAY}\

${JNI_NATIVE_METHOD}
// Step 4: Main dex and non-main dex registration functions.

namespace ${NAMESPACE} {

bool RegisterMainDexNatives(JNIEnv* env) {\
${REGISTER_MAIN_DEX_PROXY_NATIVES}
${REGISTER_MAIN_DEX_NATIVES}
  return true;
}

bool RegisterNonMainDexNatives(JNIEnv* env) {\
${REGISTER_PROXY_NATIVES}
${REGISTER_NON_MAIN_DEX_NATIVES}
  return true;
}

}  // namespace ${NAMESPACE}

#endif  // ${HEADER_GUARD}
""")
  _SetProxyRegistrationFields(registration_dict, use_hash)

  if len(registration_dict['FORWARD_DECLARATIONS']) == 0:
    return ''

  return template.substitute(registration_dict)


class HeaderGenerator(object):
  """Generates an inline header file for JNI registration."""

  def __init__(self,
               namespace,
               fully_qualified_class,
               natives,
               jni_params,
               main_dex,
               use_proxy_hash,
               enable_jni_multiplexing=False,
               switch_prefix=None):
    self.namespace = namespace
    self.natives = natives
    self.proxy_natives = [n for n in natives if n.is_proxy]
    self.non_proxy_natives = [n for n in natives if not n.is_proxy]
    self.fully_qualified_class = fully_qualified_class
    self.jni_params = jni_params
    self.class_name = self.fully_qualified_class.split('/')[-1]
    self.main_dex = main_dex
    self.helper = jni_generator.HeaderFileGeneratorHelper(
        self.class_name, fully_qualified_class, use_proxy_hash, None)
    self.use_proxy_hash = use_proxy_hash
    self.enable_jni_multiplexing = enable_jni_multiplexing
    # Each java file path is assigned a 16-bit integer as a prefix to the
    # switch number to ensure uniqueness across all native methods.
    self.switch_prefix = switch_prefix
    self.registration_dict = None

  def Generate(self):
    self.registration_dict = {'FULL_CLASS_NAME': self.fully_qualified_class}
    self._AddClassPathDeclarations()
    self._AddForwardDeclaration()
    self._AddJNINativeMethodsArrays()
    self._AddProxyNativeMethodKStrings()
    self._AddRegisterNativesCalls()
    self._AddRegisterNativesFunctions()

    self.registration_dict['PROXY_NATIVE_SIGNATURES'] = (''.join(
        _MakeProxySignature(
            native,
            self.use_proxy_hash,
            enable_jni_multiplexing=self.enable_jni_multiplexing)
        for native in self.proxy_natives))
    if self.enable_jni_multiplexing:
      self._AssignSwitchNumberToNatives()

    if self.use_proxy_hash:
      self.registration_dict['FORWARDING_PROXY_METHODS'] = ('\n'.join(
          _MakeForwardingProxy(
              native, enable_jni_multiplexing=self.enable_jni_multiplexing)
          for native in self.proxy_natives))

    return self.registration_dict

  def _SetDictValue(self, key, value):
    self.registration_dict[key] = jni_generator.WrapOutput(value)

  def _AddClassPathDeclarations(self):
    classes = self.helper.GetUniqueClasses(self.natives)
    self._SetDictValue(
        'CLASS_PATH_DECLARATIONS',
        self.helper.GetClassPathLines(classes, declare_only=True))

  def _AddForwardDeclaration(self):
    """Add the content of the forward declaration to the dictionary."""
    template = string.Template("""\
JNI_GENERATOR_EXPORT ${RETURN} ${STUB_NAME}(
    JNIEnv* env,
    ${PARAMS_IN_STUB});
""")
    forward_declaration = ''
    for native in self.natives:
      value = {
          'RETURN': jni_generator.JavaDataTypeToC(native.return_type),
          'STUB_NAME': self.helper.GetStubName(native),
          'PARAMS_IN_STUB': jni_generator.GetParamsInStub(native),
      }
      forward_declaration += template.substitute(value)
    self._SetDictValue('FORWARD_DECLARATIONS', forward_declaration)

  def _AddRegisterNativesCalls(self):
    """Add the body of the RegisterNativesImpl method to the dictionary."""

    # Only register if there is at least 1 non-proxy native
    if len(self.non_proxy_natives) == 0:
      return ''

    template = string.Template("""\
  if (!${REGISTER_NAME}(env))
    return false;
""")
    value = {
        'REGISTER_NAME':
        jni_generator.GetRegistrationFunctionName(self.fully_qualified_class)
    }
    register_body = template.substitute(value)
    if self.main_dex:
      self._SetDictValue('REGISTER_MAIN_DEX_NATIVES', register_body)
    else:
      self._SetDictValue('REGISTER_NON_MAIN_DEX_NATIVES', register_body)

  def _AddJNINativeMethodsArrays(self):
    """Returns the implementation of the array of native methods."""
    template = string.Template("""\
static const JNINativeMethod kMethods_${JAVA_CLASS}[] = {
${KMETHODS}
};

""")
    open_namespace = ''
    close_namespace = ''
    if self.namespace:
      parts = self.namespace.split('::')
      all_namespaces = ['namespace %s {' % ns for ns in parts]
      open_namespace = '\n'.join(all_namespaces) + '\n'
      all_namespaces = ['}  // namespace %s' % ns for ns in parts]
      all_namespaces.reverse()
      close_namespace = '\n'.join(all_namespaces) + '\n\n'

    body = self._SubstituteNativeMethods(template)
    self._SetDictValue('JNI_NATIVE_METHOD_ARRAY', ''.join((open_namespace, body,
                                                           close_namespace)))

  def _GetKMethodsString(self, clazz):
    ret = []
    for native in self.non_proxy_natives:
      if (native.java_class_name == clazz
          or (not native.java_class_name and clazz == self.class_name)):
        ret += [self._GetKMethodArrayEntry(native)]
    return '\n'.join(ret)

  def _GetKMethodArrayEntry(self, native):
    template = string.Template('    { "${NAME}", ${JNI_SIGNATURE}, ' +
                               'reinterpret_cast<void*>(${STUB_NAME}) },')

    name = 'native' + native.name
    if native.is_proxy:
      # Literal name of the native method in the class that contains the actual
      # native declaration.
      if self.use_proxy_hash:
        name = native.hashed_proxy_name
      else:
        name = native.proxy_name
    values = {
        'NAME':
        name,
        'JNI_SIGNATURE':
        self.jni_params.Signature(native.params, native.return_type),
        'STUB_NAME':
        self.helper.GetStubName(native)
    }
    return template.substitute(values)

  def _AddProxyNativeMethodKStrings(self):
    """Returns KMethodString for wrapped native methods in all_classes """

    if self.main_dex:
      key = 'PROXY_NATIVE_METHOD_ARRAY_MAIN_DEX'
    else:
      key = 'PROXY_NATIVE_METHOD_ARRAY'

    proxy_k_strings = ('\n'.join(
        self._GetKMethodArrayEntry(p) for p in self.proxy_natives))

    self._SetDictValue(key, proxy_k_strings)

  def _SubstituteNativeMethods(self, template, sub_proxy=False):
    """Substitutes NAMESPACE, JAVA_CLASS and KMETHODS in the provided
    template."""
    ret = []
    all_classes = self.helper.GetUniqueClasses(self.natives)
    all_classes[self.class_name] = self.fully_qualified_class

    for clazz, full_clazz in all_classes.items():
      if not sub_proxy:
        if clazz == jni_generator.ProxyHelpers.GetClass(self.use_proxy_hash):
          continue

      kmethods = self._GetKMethodsString(clazz)
      namespace_str = ''
      if self.namespace:
        namespace_str = self.namespace + '::'
      if kmethods:
        values = {
            'NAMESPACE': namespace_str,
            'JAVA_CLASS': jni_generator.EscapeClassName(full_clazz),
            'KMETHODS': kmethods
        }
        ret += [template.substitute(values)]
    if not ret: return ''
    return '\n'.join(ret)

  def GetJNINativeMethodsString(self):
    """Returns the implementation of the array of native methods."""
    template = string.Template("""\
static const JNINativeMethod kMethods_${JAVA_CLASS}[] = {
${KMETHODS}

};
""")
    return self._SubstituteNativeMethods(template)

  def _AddRegisterNativesFunctions(self):
    """Returns the code for RegisterNatives."""
    natives = self._GetRegisterNativesImplString()
    if not natives:
      return ''
    template = string.Template("""\
JNI_REGISTRATION_EXPORT bool ${REGISTER_NAME}(JNIEnv* env) {
${NATIVES}\
  return true;
}

""")
    values = {
        'REGISTER_NAME':
        jni_generator.GetRegistrationFunctionName(self.fully_qualified_class),
        'NATIVES':
        natives
    }
    self._SetDictValue('JNI_NATIVE_METHOD', template.substitute(values))

  def _GetRegisterNativesImplString(self):
    """Returns the shared implementation for RegisterNatives."""
    template = string.Template("""\
  const int kMethods_${JAVA_CLASS}Size =
      std::size(${NAMESPACE}kMethods_${JAVA_CLASS});
  if (env->RegisterNatives(
      ${JAVA_CLASS}_clazz(env),
      ${NAMESPACE}kMethods_${JAVA_CLASS},
      kMethods_${JAVA_CLASS}Size) < 0) {
    jni_generator::HandleRegistrationError(env,
        ${JAVA_CLASS}_clazz(env),
        __FILE__);
    return false;
  }

""")
    # Only register if there is a native method not in a proxy,
    # since all the proxies will be registered together.
    if len(self.non_proxy_natives) != 0:
      return self._SubstituteNativeMethods(template)
    return ''

  def _AssignSwitchNumberToNatives(self):
    # The switch number for a native method is a 32-bit integer and indicates
    # which native implementation the method should be dispatched to across
    # the JNI multiplexing boundary.
    signature_to_methods = collections.defaultdict(list)
    for native in self.proxy_natives:
      same_signature_methods = signature_to_methods[native.return_and_signature]
      # Should not exceed 65536 (2**16) methods with same proxy signature.
      assert len(same_signature_methods) < 65536

      native.switch_num = self.switch_prefix * (2**16) + len(
          same_signature_methods)
      same_signature_methods.append(native.proxy_name)


def _GetParamsListForMultiplex(params_list):
  if not params_list:
    return 'int switch_num'

  # Parameters are named after their type, with a unique number per parameter
  # type to make sure the names are unique, even within the same types.
  params_type_count = collections.defaultdict(int)
  params_with_types = []
  for p in params_list:
    params_type_count[p] += 1
    params_with_types.append(
        '%s %s_param%d' %
        (p, p.replace('[]', '_array').lower(), params_type_count[p]))

  return ', '.join(params_with_types) + ', int switch_num'


def _GetMultiplexProxyName(return_type):
  return 'resolve_for_' + return_type.replace('[]', '_array').lower()


def _MakeForwardingProxy(proxy_native, enable_jni_multiplexing=False):
  template = string.Template("""
    public static ${RETURN_TYPE} ${METHOD_NAME}(${PARAMS_WITH_TYPES}) {
        ${MAYBE_RETURN}${PROXY_CLASS}.${PROXY_METHOD_NAME}(${PARAM_NAMES});
    }""")

  params_with_types = ', '.join(
      '%s %s' % (p.datatype, p.name) for p in proxy_native.params)
  param_names = ', '.join(p.name for p in proxy_native.params)
  proxy_class = jni_generator.ProxyHelpers.GetQualifiedClass(True)

  if enable_jni_multiplexing:
    if not param_names:
      param_names = proxy_native.switch_num
    else:
      param_names += ', %s' % proxy_native.switch_num
    proxy_method_name = _GetMultiplexProxyName(proxy_native.return_type)
  else:
    proxy_method_name = proxy_native.hashed_proxy_name

  return template.substitute({
      'RETURN_TYPE':
      proxy_native.return_type,
      'METHOD_NAME':
      proxy_native.proxy_name,
      'PARAMS_WITH_TYPES':
      params_with_types,
      'MAYBE_RETURN':
      '' if proxy_native.return_type == 'void' else 'return ',
      'PROXY_CLASS':
      proxy_class.replace('/', '.'),
      'PROXY_METHOD_NAME':
      proxy_method_name,
      'PARAM_NAMES':
      param_names,
  })


def _MakeProxySignature(proxy_native,
                        use_proxy_hash,
                        enable_jni_multiplexing=False):
  params_with_types = ', '.join('%s %s' % (p.datatype, p.name)
                                for p in proxy_native.params)
  native_method_line = """
      public static native ${RETURN} ${PROXY_NAME}(${PARAMS_WITH_TYPES});"""

  if enable_jni_multiplexing:
    # This has to be only one line and without comments because all the proxy
    # signatures will be joined, then split on new lines with duplicates removed
    # since multiple |proxy_native|s map to the same multiplexed signature.
    signature_template = string.Template(native_method_line)

    alt_name = None
    return_type, params_list = proxy_native.return_and_signature
    proxy_name = _GetMultiplexProxyName(return_type)
    params_with_types = _GetParamsListForMultiplex(params_list)
  elif use_proxy_hash:
    signature_template = string.Template("""
      // Original name: ${ALT_NAME}""" + native_method_line)

    alt_name = proxy_native.proxy_name
    proxy_name = proxy_native.hashed_proxy_name
  else:
    signature_template = string.Template("""
      // Hashed name: ${ALT_NAME}""" + native_method_line)

    alt_name = proxy_native.hashed_proxy_name
    proxy_name = proxy_native.proxy_name

  return signature_template.substitute({
      'ALT_NAME': alt_name,
      'RETURN': proxy_native.return_type,
      'PROXY_NAME': proxy_name,
      'PARAMS_WITH_TYPES': params_with_types,
  })


class ProxyOptions:

  def __init__(self, **kwargs):
    self.use_hash = kwargs.get('use_hash', False)
    self.enable_jni_multiplexing = kwargs.get('enable_jni_multiplexing', False)
    self.enable_mocks = kwargs.get('enable_mocks', False)
    self.require_mocks = kwargs.get('require_mocks', False)
    # Can never require and disable.
    assert self.enable_mocks or not self.require_mocks


def main(argv):
  arg_parser = argparse.ArgumentParser()
  build_utils.AddDepfileOption(arg_parser)

  arg_parser.add_argument(
      '--sources-files',
      required=True,
      action='append',
      help='A list of .sources files which contain Java '
      'file paths.')
  arg_parser.add_argument(
      '--header-path', help='Path to output header file (optional).')
  arg_parser.add_argument(
      '--srcjar-path',
      required=True,
      help='Path to output srcjar for GEN_JNI.java (and J/N.java if proxy'
      ' hash is enabled).')
  arg_parser.add_argument(
      '--sources-exclusions',
      default=[],
      help='A list of Java files which should be ignored '
      'by the parser.')
  arg_parser.add_argument(
      '--namespace',
      default='',
      help='Namespace to wrap the registration functions '
      'into.')
  # TODO(crbug.com/898261) hook these flags up to the build config to enable
  # mocking in instrumentation tests
  arg_parser.add_argument(
      '--enable_proxy_mocks',
      default=False,
      action='store_true',
      help='Allows proxy native impls to be mocked through Java.')
  arg_parser.add_argument(
      '--require_mocks',
      default=False,
      action='store_true',
      help='Requires all used native implementations to have a mock set when '
      'called. Otherwise an exception will be thrown.')
  arg_parser.add_argument(
      '--use_proxy_hash',
      action='store_true',
      help='Enables hashing of the native declaration for methods in '
      'an @JniNatives interface')
  arg_parser.add_argument(
      '--enable_jni_multiplexing',
      action='store_true',
      help='Enables JNI multiplexing for Java native methods')
  args = arg_parser.parse_args(build_utils.ExpandFileArgs(argv[1:]))

  if not args.enable_proxy_mocks and args.require_mocks:
    arg_parser.error(
        'Invalid arguments: --require_mocks without --enable_proxy_mocks. '
        'Cannot require mocks if they are not enabled.')

  sources_files = sorted(set(build_utils.ParseGnList(args.sources_files)))
  proxy_opts = ProxyOptions(
      use_hash=args.use_proxy_hash,
      enable_jni_multiplexing=args.enable_jni_multiplexing,
      require_mocks=args.require_mocks,
      enable_mocks=args.enable_proxy_mocks)

  java_file_paths = []
  for f in sources_files:
    # Skip generated files, since the GN targets do not declare any deps.
    java_file_paths.extend(
        p for p in build_utils.ReadSourcesList(f)
        if p.startswith('..') and p not in args.sources_exclusions)
  _Generate(
      java_file_paths,
      args.srcjar_path,
      proxy_opts=proxy_opts,
      header_path=args.header_path,
      namespace=args.namespace)

  if args.depfile:
    build_utils.WriteDepfile(args.depfile, args.srcjar_path,
                             sources_files + java_file_paths)


if __name__ == '__main__':
  sys.exit(main(sys.argv))
