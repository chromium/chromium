#!/usr/bin/env python3
# Copyright 2017 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Generates GEN_JNI.java (or N.java) and optional header for manual JNI
registration.
"""

import argparse
import collections
import functools
import hashlib
import multiprocessing
import os
import re
import string
import sys
import zipfile

import jni_generator
from util import build_utils
import action_helpers  # build_utils adds //build to sys.path.
import zip_helpers

# All but FULL_CLASS_NAME, which is used only for sorting.
MERGEABLE_KEYS = [
    'CLASS_PATH_DECLARATIONS',
    'FORWARD_DECLARATIONS',
    'JNI_NATIVE_METHOD',
    'JNI_NATIVE_METHOD_ARRAY',
    'PROXY_NATIVE_SIGNATURES',
    'FORWARDING_PROXY_METHODS',
    'PROXY_NATIVE_METHOD_ARRAY',
    'REGISTER_NATIVES',
]


def _Generate(options, java_file_paths):
  """Generates files required to perform JNI registration.

  Generates a srcjar containing a single class, GEN_JNI, that contains all
  native method declarations.

  Optionally generates a header file that provides RegisterNatives to perform
  JNI registration.

  Args:
    options: arguments from the command line
    java_file_paths: A list of java file paths.
  """
  # Without multiprocessing, script takes ~13 seconds for chrome_public_apk
  # on a z620. With multiprocessing, takes ~2 seconds.
  results = collections.defaultdict(list)
  with multiprocessing.Pool() as pool:
    for d in pool.imap_unordered(functools.partial(_DictForPath, options),
                                 java_file_paths):
      if d:
        results[d['MODULE_NAME']].append(d)

  combined_dicts = collections.defaultdict(dict)
  for module_name, module_results in results.items():
    # Sort to make output deterministic.
    module_results.sort(key=lambda d: d['FULL_CLASS_NAME'])
    combined_dict = combined_dicts[module_name]
    for key in MERGEABLE_KEYS:
      combined_dict[key] = ''.join(d.get(key, '') for d in module_results)

    # PROXY_NATIVE_SIGNATURES and PROXY_NATIVE_METHOD_ARRAY will have
    # duplicates for JNI multiplexing since all native methods with similar
    # signatures map to the same proxy. Similarly, there may be multiple switch
    # case entries for the same proxy signatures.
    if options.enable_jni_multiplexing:
      proxy_signatures_list = sorted(
          set(combined_dict['PROXY_NATIVE_SIGNATURES'].split('\n')))
      combined_dict['PROXY_NATIVE_SIGNATURES'] = '\n'.join(
          signature for signature in proxy_signatures_list)

      proxy_native_array_list = sorted(
          set(combined_dict['PROXY_NATIVE_METHOD_ARRAY'].split('},\n')))
      combined_dict['PROXY_NATIVE_METHOD_ARRAY'] = '},\n'.join(
          p for p in proxy_native_array_list if p != '') + '}'

      signature_to_cases = collections.defaultdict(list)
      for d in module_results:
        for signature, cases in d['SIGNATURE_TO_CASES'].items():
          signature_to_cases[signature].extend(cases)
      combined_dict['FORWARDING_CALLS'] = _AddForwardingCalls(
          signature_to_cases, module_name, options.package_prefix)

  if options.header_path:
    assert len(
        combined_dicts) == 1, 'Cannot output a header for multiple modules'
    module_name = next(iter(combined_dicts))
    combined_dict = combined_dicts[module_name]

    header_guard = os.path.splitext(options.header_path)[0].upper() + '_'
    header_guard = re.sub(r'[/.-]', '_', header_guard)
    combined_dict['HEADER_GUARD'] = header_guard
    combined_dict['NAMESPACE'] = options.namespace
    header_content = CreateFromDict(options, module_name, combined_dict)
    with action_helpers.atomic_output(options.header_path, mode='w') as f:
      f.write(header_content)

  with action_helpers.atomic_output(options.srcjar_path) as f:
    with zipfile.ZipFile(f, 'w') as srcjar:
      for module_name, combined_dict in combined_dicts.items():

        if options.use_proxy_hash or options.enable_jni_multiplexing:
          # J/N.java
          zip_helpers.add_to_zip_hermetic(
              srcjar,
              '%s.java' % jni_generator.ProxyHelpers.GetQualifiedClass(
                  True, module_name, options.package_prefix),
              data=CreateProxyJavaFromDict(options, module_name, combined_dict))
          # org/chromium/base/natives/GEN_JNI.java
          zip_helpers.add_to_zip_hermetic(
              srcjar,
              '%s.java' % jni_generator.ProxyHelpers.GetQualifiedClass(
                  False, module_name, options.package_prefix),
              data=CreateProxyJavaFromDict(options,
                                           module_name,
                                           combined_dict,
                                           forwarding=True))
        else:
          # org/chromium/base/natives/GEN_JNI.java
          zip_helpers.add_to_zip_hermetic(
              srcjar,
              '%s.java' % jni_generator.ProxyHelpers.GetQualifiedClass(
                  False, module_name, options.package_prefix),
              data=CreateProxyJavaFromDict(options, module_name, combined_dict))


def _DictForPath(options, path):
  with open(path) as f:
    contents = jni_generator.RemoveComments(f.read())
    if '@JniIgnoreNatives' in contents:
      return None

  fully_qualified_class = jni_generator.ExtractFullyQualifiedJavaClassName(
      path, contents)

  if options.package_prefix:
    fully_qualified_class = jni_generator.GetFullyQualifiedClassWithPackagePrefix(
        fully_qualified_class, options.package_prefix)

  natives, found_module_name = jni_generator.ProxyHelpers.ExtractStaticProxyNatives(
      fully_qualified_class=fully_qualified_class,
      contents=contents,
      ptr_type='long',
      include_test_only=options.include_test_only)

  if options.module_name and found_module_name != options.module_name:
    # Ignoring any code from modules we aren't looking at.
    return None

  natives += jni_generator.ExtractNatives(contents, 'long')

  if len(natives) == 0:
    return None
  # The namespace for the content is separate from the namespace for the
  # generated header file.
  content_namespace = jni_generator.ExtractJNINamespace(contents)
  jni_params = jni_generator.JniParams(fully_qualified_class)
  jni_params.ExtractImportsAndInnerClasses(contents)
  dict_generator = DictionaryGenerator(options, found_module_name,
                                       content_namespace, fully_qualified_class,
                                       natives, jni_params)
  return dict_generator.Generate()


def _AddForwardingCalls(signature_to_cases, module_name, package_prefix):
  template = string.Template("""
JNI_GENERATOR_EXPORT ${RETURN} Java_${CLASS_NAME}_${PROXY_SIGNATURE}(
    JNIEnv* env,
    jclass jcaller,
    ${PARAMS_IN_STUB}) {
        switch (switch_num) {
          ${CASES}
          default:
            CHECK(false) << "JNI multiplexing function Java_\
${CLASS_NAME}_${PROXY_SIGNATURE} was called with an invalid switch number: "\
 << switch_num;
            return${DEFAULT_RETURN};
        }
}""")

  switch_statements = []
  for signature, cases in sorted(signature_to_cases.items()):
    return_type, params_list = signature
    params_in_stub = _GetJavaToNativeParamsList(params_list)
    switch_statements.append(
        template.substitute({
            'RETURN':
            jni_generator.JavaDataTypeToC(return_type),
            'CLASS_NAME':
            jni_generator.EscapeClassName(
                jni_generator.ProxyHelpers.GetQualifiedClass(
                    True, module_name, package_prefix)),
            'PROXY_SIGNATURE':
            jni_generator.EscapeClassName(
                _GetMultiplexProxyName(return_type, params_list)),
            'PARAMS_IN_STUB':
            params_in_stub,
            'CASES':
            ''.join(cases),
            'DEFAULT_RETURN':
            '' if return_type == 'void' else ' {}',
        }))

  return ''.join(s for s in switch_statements)


def _SetProxyRegistrationFields(options, module_name, registration_dict):
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

  manual_registration = string.Template("""\
// Step 3: Method declarations.

${JNI_NATIVE_METHOD_ARRAY}\
${PROXY_NATIVE_METHOD_ARRAY}\

${JNI_NATIVE_METHOD}
// Step 4: Registration function.

namespace ${NAMESPACE} {

bool RegisterNatives(JNIEnv* env) {\
${REGISTER_PROXY_NATIVES}
${REGISTER_NATIVES}
  return true;
}

}  // namespace ${NAMESPACE}
""")

  short_name = options.use_proxy_hash or options.enable_jni_multiplexing
  sub_dict = {
      'ESCAPED_PROXY_CLASS':
      jni_generator.EscapeClassName(
          jni_generator.ProxyHelpers.GetQualifiedClass(short_name, module_name,
                                                       options.package_prefix)),
      'PROXY_CLASS':
      jni_generator.ProxyHelpers.GetQualifiedClass(short_name, module_name,
                                                   options.package_prefix),
      'KMETHODS':
      registration_dict['PROXY_NATIVE_METHOD_ARRAY'],
      'REGISTRATION_NAME':
      jni_generator.GetRegistrationFunctionName(
          jni_generator.ProxyHelpers.GetQualifiedClass(short_name, module_name,
                                                       options.package_prefix)),
  }

  if registration_dict['PROXY_NATIVE_METHOD_ARRAY']:
    proxy_native_array = registration_template.substitute(sub_dict)
    proxy_natives_registration = registration_call.substitute(sub_dict)
  else:
    proxy_native_array = ''
    proxy_natives_registration = ''

  registration_dict['PROXY_NATIVE_METHOD_ARRAY'] = proxy_native_array
  registration_dict['REGISTER_PROXY_NATIVES'] = proxy_natives_registration

  if options.manual_jni_registration:
    registration_dict['MANUAL_REGISTRATION'] = manual_registration.substitute(
        registration_dict)
  else:
    registration_dict['MANUAL_REGISTRATION'] = ''


def CreateProxyJavaFromDict(options,
                            module_name,
                            registration_dict,
                            forwarding=False):
  template = string.Template("""\
// Copyright 2018 The Chromium Authors
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

  is_natives_class = not forwarding and (options.use_proxy_hash
                                         or options.enable_jni_multiplexing)
  class_name = jni_generator.ProxyHelpers.GetClass(is_natives_class,
                                                   module_name)
  package = jni_generator.ProxyHelpers.GetPackage(is_natives_class,
                                                  options.package_prefix)

  if forwarding or not (options.use_proxy_hash
                        or options.enable_jni_multiplexing):
    fields = string.Template("""\
    public static final boolean TESTING_ENABLED = ${TESTING_ENABLED};
    public static final boolean REQUIRE_MOCK = ${REQUIRE_MOCK};
""").substitute({
        'TESTING_ENABLED': str(options.enable_proxy_mocks).lower(),
        'REQUIRE_MOCK': str(options.require_mocks).lower(),
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


def CreateFromDict(options, module_name, registration_dict):
  """Returns the content of the header file."""

  template = string.Template("""\
// Copyright 2017 The Chromium Authors
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
${FORWARDING_CALLS}
${MANUAL_REGISTRATION}
#endif  // ${HEADER_GUARD}
""")
  _SetProxyRegistrationFields(options, module_name, registration_dict)
  if not options.enable_jni_multiplexing:
    registration_dict['FORWARDING_CALLS'] = ''
  if len(registration_dict['FORWARD_DECLARATIONS']) == 0:
    return ''

  return template.substitute(registration_dict)


def _GetJavaToNativeParamsList(params_list):
  if not params_list:
    return 'jlong switch_num'

  # Parameters are named after their type, with a unique number per parameter
  # type to make sure the names are unique, even within the same types.
  params_type_count = collections.defaultdict(int)
  params_in_stub = []
  for p in params_list:
    params_type_count[p] += 1
    params_in_stub.append(
        '%s %s_param%d' %
        (jni_generator.JavaDataTypeToC(p), p.replace(
            '[]', '_array').lower(), params_type_count[p]))

  return 'jlong switch_num, ' + ', '.join(params_in_stub)


class DictionaryGenerator(object):
  """Generates an inline header file for JNI registration."""

  def __init__(self, options, module_name, content_namespace,
               fully_qualified_class, natives, jni_params):
    self.options = options
    self.module_name = module_name
    self.content_namespace = content_namespace
    self.natives = natives
    self.proxy_natives = [n for n in natives if n.is_proxy]
    self.non_proxy_natives = [n for n in natives if not n.is_proxy]
    self.fully_qualified_class = fully_qualified_class
    self.jni_params = jni_params
    self.class_name = self.fully_qualified_class.split('/')[-1]
    self.helper = jni_generator.HeaderFileGeneratorHelper(
        self.class_name,
        self.module_name,
        fully_qualified_class,
        options.use_proxy_hash,
        options.package_prefix,
        enable_jni_multiplexing=options.enable_jni_multiplexing)
    self.registration_dict = None

  def Generate(self):
    self.registration_dict = {
        'FULL_CLASS_NAME': self.fully_qualified_class,
        'MODULE_NAME': self.module_name
    }
    self._AddClassPathDeclarations()
    self._AddForwardDeclaration()
    self._AddJNINativeMethodsArrays()
    self._AddProxyNativeMethodKStrings()
    self._AddRegisterNativesCalls()
    self._AddRegisterNativesFunctions()

    self.registration_dict['PROXY_NATIVE_SIGNATURES'] = (''.join(
        _MakeProxySignature(self.options, native)
        for native in self.proxy_natives))

    if self.options.enable_jni_multiplexing:
      self._AssignSwitchNumberToNatives()
      self._AddCases()

    if self.options.use_proxy_hash or self.options.enable_jni_multiplexing:
      self.registration_dict['FORWARDING_PROXY_METHODS'] = ('\n'.join(
          _MakeForwardingProxy(self.options, self.module_name, native)
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
    self._SetDictValue('REGISTER_NON_NATIVES', register_body)

  def _AddJNINativeMethodsArrays(self):
    """Returns the implementation of the array of native methods."""
    template = string.Template("""\
static const JNINativeMethod kMethods_${JAVA_CLASS}[] = {
${KMETHODS}
};

""")
    open_namespace = ''
    close_namespace = ''
    if self.content_namespace:
      parts = self.content_namespace.split('::')
      all_namespaces = ['namespace %s {' % ns for ns in parts]
      open_namespace = '\n'.join(all_namespaces) + '\n'
      all_namespaces = ['}  // namespace %s' % ns for ns in parts]
      all_namespaces.reverse()
      close_namespace = '\n'.join(all_namespaces) + '\n\n'

    body = self._SubstituteNativeMethods(template)
    if body:
      self._SetDictValue('JNI_NATIVE_METHOD_ARRAY', ''.join(
          (open_namespace, body, close_namespace)))

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
    jni_signature = self.jni_params.Signature(native.params, native.return_type)
    stub_name = self.helper.GetStubName(native)

    if native.is_proxy:
      # Literal name of the native method in the class that contains the actual
      # native declaration.
      if self.options.enable_jni_multiplexing:
        return_type, params_list = native.return_and_signature
        class_name = jni_generator.EscapeClassName(
            jni_generator.ProxyHelpers.GetQualifiedClass(
                True, self.module_name, self.options.package_prefix))
        proxy_signature = jni_generator.EscapeClassName(
            _GetMultiplexProxyName(return_type, params_list))

        name = _GetMultiplexProxyName(return_type, params_list)
        jni_signature = self.jni_params.Signature(
            [jni_generator.Param(datatype='long', name='switch_num')] +
            native.params, native.return_type)
        stub_name = 'Java_' + class_name + '_' + proxy_signature
      elif self.options.use_proxy_hash:
        name = native.hashed_proxy_name
      else:
        name = native.proxy_name
    values = {
        'NAME': name,
        'JNI_SIGNATURE': jni_signature,
        'STUB_NAME': stub_name
    }
    return template.substitute(values)

  def _AddProxyNativeMethodKStrings(self):
    """Returns KMethodString for wrapped native methods in all_classes """

    proxy_k_strings = ('\n'.join(
        self._GetKMethodArrayEntry(p) for p in self.proxy_natives))

    self._SetDictValue('PROXY_NATIVE_METHOD_ARRAY', proxy_k_strings)

  def _SubstituteNativeMethods(self, template):
    """Substitutes NAMESPACE, JAVA_CLASS and KMETHODS in the provided
    template."""
    ret = []
    all_classes = self.helper.GetUniqueClasses(self.natives)
    all_classes[self.class_name] = self.fully_qualified_class

    for clazz, full_clazz in all_classes.items():
      if clazz == jni_generator.ProxyHelpers.GetClass(
          self.options.use_proxy_hash or self.options.enable_jni_multiplexing,
          self.module_name):
        continue

      kmethods = self._GetKMethodsString(clazz)
      namespace_str = ''
      if self.content_namespace:
        namespace_str = self.content_namespace + '::'
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
    # The switch number for a native method is a 64-bit long with the first
    # bit being a sign digit. The signed two's complement is taken when
    # appropriate to make use of negative numbers.
    for native in self.proxy_natives:
      hashed_long = hashlib.md5(
          native.proxy_name.encode('utf-8')).hexdigest()[:16]
      switch_num = int(hashed_long, 16)
      if (switch_num & 1 << 63):
        switch_num -= (1 << 64)

      native.switch_num = str(switch_num)

  def _AddCases(self):
    # Switch cases are grouped together by the same proxy signatures.
    template = string.Template("""
          case ${SWITCH_NUM}:
            return ${STUB_NAME}(env, jcaller${PARAMS});
          """)

    signature_to_cases = collections.defaultdict(list)
    for native in self.proxy_natives:
      signature = native.return_and_signature
      params = _GetParamsListForMultiplex(signature[1], with_types=False)
      values = {
          'SWITCH_NUM': native.switch_num,
          # We are forced to call the generated stub instead of the impl because
          # the impl is not guaranteed to have a globally unique name.
          'STUB_NAME': self.helper.GetStubName(native),
          'PARAMS': params,
      }
      signature_to_cases[signature].append(template.substitute(values))

    self.registration_dict['SIGNATURE_TO_CASES'] = signature_to_cases


def _GetParamsListForMultiplex(params_list, with_types):
  if not params_list:
    return ''

  # Parameters are named after their type, with a unique number per parameter
  # type to make sure the names are unique, even within the same types.
  params_type_count = collections.defaultdict(int)
  params = []
  for p in params_list:
    params_type_count[p] += 1
    param_type = p + ' ' if with_types else ''
    params.append(
        '%s%s_param%d' %
        (param_type, p.replace('[]', '_array').lower(), params_type_count[p]))

  return ', ' + ', '.join(params)


def _GetMultiplexProxyName(return_type, params_list):
  # Proxy signatures for methods are named after their return type and
  # parameters to ensure uniqueness, even for the same return types.
  params = ''
  if params_list:
    type_convert_dictionary = {
        '[]': 'A',
        'byte': 'B',
        'char': 'C',
        'double': 'D',
        'float': 'F',
        'int': 'I',
        'long': 'J',
        'Class': 'L',
        'Object': 'O',
        'String': 'R',
        'short': 'S',
        'Throwable': 'T',
        'boolean': 'Z',
    }
    # Parameter types could contain multi-dimensional arrays and every
    # instance of [] has to be replaced in the proxy signature name.
    for k, v in type_convert_dictionary.items():
      params_list = [p.replace(k, v) for p in params_list]
    params = '_' + ''.join(p for p in params_list)

  return 'resolve_for_' + return_type.replace('[]', '_array').lower() + params


def _MakeForwardingProxy(options, module_name, proxy_native):
  template = string.Template("""
    public static ${RETURN_TYPE} ${METHOD_NAME}(${PARAMS_WITH_TYPES}) {
        ${MAYBE_RETURN}${PROXY_CLASS}.${PROXY_METHOD_NAME}(${PARAM_NAMES});
    }""")

  params_with_types = ', '.join(
      '%s %s' % (p.datatype, p.name) for p in proxy_native.params)
  param_names = ', '.join(p.name for p in proxy_native.params)
  proxy_class = jni_generator.ProxyHelpers.GetQualifiedClass(
      True, module_name, options.package_prefix)

  if options.enable_jni_multiplexing:
    if not param_names:
      param_names = proxy_native.switch_num + 'L'
    else:
      param_names = proxy_native.switch_num + 'L, ' + param_names
    return_type, params_list = proxy_native.return_and_signature
    proxy_method_name = _GetMultiplexProxyName(return_type, params_list)
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


def _MakeProxySignature(options, proxy_native):
  params_with_types = ', '.join('%s %s' % (p.datatype, p.name)
                                for p in proxy_native.params)
  native_method_line = """
      public static native ${RETURN} ${PROXY_NAME}(${PARAMS_WITH_TYPES});"""

  if options.enable_jni_multiplexing:
    # This has to be only one line and without comments because all the proxy
    # signatures will be joined, then split on new lines with duplicates removed
    # since multiple |proxy_native|s map to the same multiplexed signature.
    signature_template = string.Template(native_method_line)

    alt_name = None
    return_type, params_list = proxy_native.return_and_signature
    proxy_name = _GetMultiplexProxyName(return_type, params_list)
    params_with_types = 'long switch_num' + _GetParamsListForMultiplex(
        params_list, with_types=True)
  elif options.use_proxy_hash:
    signature_template = string.Template("""
      // Original name: ${ALT_NAME}""" + native_method_line)

    alt_name = proxy_native.proxy_name
    proxy_name = proxy_native.hashed_proxy_name
  else:
    signature_template = string.Template("""
      // Hashed name: ${ALT_NAME}""" + native_method_line)

    # We add the prefix that is sometimes used so that codesearch can find it if
    # someone searches a full method name from the stacktrace.
    alt_name = f'Java_J_N_{proxy_native.hashed_proxy_name}'
    proxy_name = proxy_native.proxy_name

  return signature_template.substitute({
      'ALT_NAME': alt_name,
      'RETURN': proxy_native.return_type,
      'PROXY_NAME': proxy_name,
      'PARAMS_WITH_TYPES': params_with_types,
  })


def main(argv):
  arg_parser = argparse.ArgumentParser()
  action_helpers.add_depfile_arg(arg_parser)

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
  arg_parser.add_argument('--file-exclusions',
                          default=[],
                          help='A list of Java files which should be ignored '
                          'by the parser.')
  arg_parser.add_argument(
      '--namespace',
      default='',
      help='Native namespace to wrap the registration functions '
      'into.')
  # TODO(crbug.com/898261) hook these flags up to the build config to enable
  # mocking in instrumentation tests
  arg_parser.add_argument(
      '--enable-proxy-mocks',
      default=False,
      action='store_true',
      help='Allows proxy native impls to be mocked through Java.')
  arg_parser.add_argument(
      '--require-mocks',
      default=False,
      action='store_true',
      help='Requires all used native implementations to have a mock set when '
      'called. Otherwise an exception will be thrown.')
  arg_parser.add_argument(
      '--use-proxy-hash',
      action='store_true',
      help='Enables hashing of the native declaration for methods in '
      'an @JniNatives interface')
  arg_parser.add_argument(
      '--module-name',
      default='',
      help='Only look at natives annotated with a specific module name.')
  arg_parser.add_argument(
      '--enable-jni-multiplexing',
      action='store_true',
      help='Enables JNI multiplexing for Java native methods')
  arg_parser.add_argument(
      '--manual-jni-registration',
      action='store_true',
      help='Manually do JNI registration - required for crazy linker')
  arg_parser.add_argument('--include-test-only',
                          action='store_true',
                          help='Whether to maintain ForTesting JNI methods.')
  arg_parser.add_argument(
      '--package_prefix',
      help=
      'Adds a prefix to the classes fully qualified-name. Effectively changing a class name from'
      'foo.bar -> prefix.foo.bar')
  args = arg_parser.parse_args(build_utils.ExpandFileArgs(argv[1:]))

  if not args.enable_proxy_mocks and args.require_mocks:
    arg_parser.error(
        'Invalid arguments: --require-mocks without --enable-proxy-mocks. '
        'Cannot require mocks if they are not enabled.')
  if not args.header_path and args.manual_jni_registration:
    arg_parser.error(
        'Invalid arguments: --manual-jni-registration without --header-path. '
        'Cannot manually register JNI if there is no output header file.')

  sources_files = sorted(set(action_helpers.parse_gn_list(args.sources_files)))

  java_file_paths = []
  for f in sources_files:
    # Skip generated files, since the GN targets do not declare any deps. Also
    # skip Kotlin files as they are not supported by JNI generation.
    java_file_paths.extend(
        p for p in build_utils.ReadSourcesList(f) if p.startswith('..')
        and p not in args.file_exclusions and not p.endswith('.kt'))
  _Generate(args, java_file_paths)

  if args.depfile:
    action_helpers.write_depfile(args.depfile, args.srcjar_path,
                                 sources_files + java_file_paths)


if __name__ == '__main__':
  sys.exit(main(sys.argv))
