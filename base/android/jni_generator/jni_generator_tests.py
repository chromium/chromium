#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Tests for jni_generator.py.

This test suite contains various tests for the JNI generator.
It exercises the low-level parser all the way up to the
code generator and ensures the output matches a golden
file.
"""

from __future__ import print_function

import difflib
import inspect
import optparse
import os
import sys
import unittest
import jni_generator
import jni_registration_generator
from jni_generator import CalledByNative
from jni_generator import IsMainDexJavaClass
from jni_generator import NativeMethod
from jni_generator import Param
from jni_generator import ProxyHelpers

_SCRIPT_NAME = 'base/android/jni_generator/jni_generator.py'
_INCLUDES = ('base/android/jni_generator/jni_generator_helper.h')
_JAVA_SRC_DIR = os.path.join('java', 'src', 'org', 'chromium', 'example',
                             'jni_generator')

# Set this environment variable in order to regenerate the golden text
# files.
_REBASELINE_ENV = 'REBASELINE'


class TestOptions(object):
  """The mock options object which is passed to the jni_generator.py script."""

  def __init__(self):
    self.namespace = None
    self.script_name = _SCRIPT_NAME
    self.includes = _INCLUDES
    self.ptr_type = 'long'
    self.cpp = 'cpp'
    self.javap = 'javap'
    self.native_exports_optional = True
    self.enable_profiling = False
    self.enable_tracing = False
    self.use_proxy_hash = False
    self.always_mangle = False


class BaseTest(unittest.TestCase):

  @staticmethod
  def _MergeRegistrationForTests(results,
                                 header_guard='HEADER_GUARD',
                                 namespace='test'):

    results.sort(key=lambda d: d['FULL_CLASS_NAME'])

    combined_dict = {}
    for key in jni_registration_generator.MERGEABLE_KEYS:
      combined_dict[key] = ''.join(d.get(key, '') for d in results)

    combined_dict['HEADER_GUARD'] = header_guard
    combined_dict['NAMESPACE'] = namespace
    return combined_dict

  def _JoinScriptDir(self, path):
    script_dir = os.path.dirname(sys.argv[0])
    return os.path.join(script_dir, path)

  def _JoinGoldenPath(self, golden_file_name):
    return self._JoinScriptDir(os.path.join('golden', golden_file_name))

  def _ReadGoldenFile(self, golden_file_name):
    golden_file_name = self._JoinGoldenPath(golden_file_name)
    if not os.path.exists(golden_file_name):
      return None
    with open(golden_file_name, 'r') as f:
      return f.read()

  def _CreateJniHeaderFromFile(self, fname, qualified_clazz, options=None):
    with open(self._JoinScriptDir(fname)) as f:
      content = f.read()
    opts = options
    if opts is None:
      opts = TestOptions()

    jni_from_java = jni_generator.JNIFromJavaSource(content, qualified_clazz,
                                                    opts)
    return jni_from_java.GetContent()

  def AssertObjEquals(self, first, second):
    if isinstance(first, str):
      return self.assertEqual(first, second)
    dict_first = first.__dict__
    dict_second = second.__dict__
    self.assertEqual(dict_first.keys(), dict_second.keys())
    for key, value in dict_first.items():
      if (type(value) is list and len(value)
          and isinstance(type(value[0]), object)):
        self.AssertListEquals(value, second.__getattribute__(key))
      else:
        actual = second.__getattribute__(key)
        self.assertEqual(value, actual,
                         'Key ' + key + ': ' + str(value) + '!=' + str(actual))

  def AssertListEquals(self, first, second):
    self.assertEqual(len(first), len(second))
    for i in range(len(first)):
      if isinstance(first[i], object):
        self.AssertObjEquals(first[i], second[i])
      else:
        self.assertEqual(first[i], second[i])

  def AssertTextEquals(self, golden_text, generated_text):
    if not self.CompareText(golden_text, generated_text):
      self.fail('Golden text mismatch.')

  def CompareText(self, golden_text, generated_text):

    def FilterText(text):
      return [
          l.strip() for l in text.split('\n')
          if not l.startswith('// Copyright')
      ]

    stripped_golden = FilterText(golden_text)
    stripped_generated = FilterText(generated_text)
    if stripped_golden == stripped_generated:
      return True
    print(self.id())
    for line in difflib.context_diff(stripped_golden, stripped_generated):
      print(line)
    print('\n\nGenerated')
    print('=' * 80)
    print(generated_text)
    print('=' * 80)
    print('Run with:')
    print('REBASELINE=1', sys.argv[0])
    print('to regenerate the data files.')

  def AssertGoldenTextEquals(self, generated_text, suffix='', golden_file=None):
    """Compares generated text with the corresponding golden_file

    By default compares generated_text with the file at
    script_dir/golden/{caller_name}[suffix].golden. If the parameter
    golden_file is provided it will instead compare the generated text with
    script_dir/golden/golden_file."""
    # This is the caller test method.
    caller = inspect.stack()[1][3]

    if golden_file is None:
      self.assertTrue(
          caller.startswith('test'),
          'AssertGoldenTextEquals can only be called from a '
          'test* method, not %s' % caller)
      golden_file = '%s%s.golden' % (caller, suffix)
    golden_text = self._ReadGoldenFile(golden_file)
    if os.environ.get(_REBASELINE_ENV):
      if golden_text != generated_text:
        with open(self._JoinGoldenPath(golden_file), 'w') as f:
          f.write(generated_text)
      return
    # golden_text is None if no file is found. Better to fail than in
    # AssertTextEquals so we can give a clearer message.
    if golden_text is None:
      self.fail(
          'Golden file %s does not exist.' % self._JoinGoldenPath(golden_file))
    self.AssertTextEquals(golden_text, generated_text)


class TestGenerator(BaseTest):

  def testInspectCaller(self):

    def willRaise():
      # This function can only be called from a test* method.
      self.AssertGoldenTextEquals('')

    self.assertRaises(AssertionError, willRaise)

  def testNatives(self):
    test_data = """"
    import android.graphics.Bitmap;
    import android.view.View;

    interface OnFrameAvailableListener {}
    private native int nativeInit();
    private native void nativeDestroy(int nativeChromeBrowserProvider);
    private native long nativeAddBookmark(
            int nativeChromeBrowserProvider,
            String url, String title, boolean isFolder, long parentId);
    private static native String nativeGetDomainAndRegistry(String url);
    private static native void nativeCreateHistoricalTabFromState(
            byte[] state, int tab_index);
    private native byte[] nativeGetStateAsByteArray(View view);
    private static native String[] nativeGetAutofillProfileGUIDs();
    private native void nativeSetRecognitionResults(
            int sessionId, String[] results);
    private native long nativeAddBookmarkFromAPI(
            int nativeChromeBrowserProvider,
            String url, Long created, Boolean isBookmark,
            Long date, byte[] favicon, String title, Integer visits);
    native int nativeFindAll(String find);
    private static native OnFrameAvailableListener nativeGetInnerClass();
    private native Bitmap nativeQueryBitmap(
            int nativeChromeBrowserProvider,
            String[] projection, String selection,
            String[] selectionArgs, String sortOrder);
    private native void nativeGotOrientation(
            int nativeDataFetcherImplAndroid,
            double alpha, double beta, double gamma);
    private static native Throwable nativeMessWithJavaException(Throwable e);
    """
    jni_params = jni_generator.JniParams(
        'org/chromium/example/jni_generator/SampleForTests')
    jni_params.ExtractImportsAndInnerClasses(test_data)
    natives = jni_generator.ExtractNatives(test_data, 'int')
    golden_natives = [
        NativeMethod(
            return_type='int',
            static=False,
            name='Init',
            params=[],
            java_class_name=None),
        NativeMethod(
            return_type='void',
            static=False,
            name='Destroy',
            params=[Param(datatype='int', name='nativeChromeBrowserProvider')],
            java_class_name=None),
        NativeMethod(
            return_type='long',
            static=False,
            name='AddBookmark',
            params=[
                Param(datatype='int', name='nativeChromeBrowserProvider'),
                Param(datatype='String', name='url'),
                Param(datatype='String', name='title'),
                Param(datatype='boolean', name='isFolder'),
                Param(datatype='long', name='parentId')
            ],
            java_class_name=None),
        NativeMethod(
            return_type='String',
            static=True,
            name='GetDomainAndRegistry',
            params=[Param(datatype='String', name='url')],
            java_class_name=None),
        NativeMethod(
            return_type='void',
            static=True,
            name='CreateHistoricalTabFromState',
            params=[
                Param(datatype='byte[]', name='state'),
                Param(datatype='int', name='tab_index')
            ],
            java_class_name=None),
        NativeMethod(
            return_type='byte[]',
            static=False,
            name='GetStateAsByteArray',
            params=[Param(datatype='View', name='view')],
            java_class_name=None),
        NativeMethod(
            return_type='String[]',
            static=True,
            name='GetAutofillProfileGUIDs',
            params=[],
            java_class_name=None),
        NativeMethod(
            return_type='void',
            static=False,
            name='SetRecognitionResults',
            params=[
                Param(datatype='int', name='sessionId'),
                Param(datatype='String[]', name='results')
            ],
            java_class_name=None),
        NativeMethod(
            return_type='long',
            static=False,
            name='AddBookmarkFromAPI',
            params=[
                Param(datatype='int', name='nativeChromeBrowserProvider'),
                Param(datatype='String', name='url'),
                Param(datatype='Long', name='created'),
                Param(datatype='Boolean', name='isBookmark'),
                Param(datatype='Long', name='date'),
                Param(datatype='byte[]', name='favicon'),
                Param(datatype='String', name='title'),
                Param(datatype='Integer', name='visits')
            ],
            java_class_name=None),
        NativeMethod(
            return_type='int',
            static=False,
            name='FindAll',
            params=[Param(datatype='String', name='find')],
            java_class_name=None),
        NativeMethod(
            return_type='OnFrameAvailableListener',
            static=True,
            name='GetInnerClass',
            params=[],
            java_class_name=None),
        NativeMethod(
            return_type='Bitmap',
            static=False,
            name='QueryBitmap',
            params=[
                Param(datatype='int', name='nativeChromeBrowserProvider'),
                Param(datatype='String[]', name='projection'),
                Param(datatype='String', name='selection'),
                Param(datatype='String[]', name='selectionArgs'),
                Param(datatype='String', name='sortOrder'),
            ],
            java_class_name=None),
        NativeMethod(
            return_type='void',
            static=False,
            name='GotOrientation',
            params=[
                Param(datatype='int', name='nativeDataFetcherImplAndroid'),
                Param(datatype='double', name='alpha'),
                Param(datatype='double', name='beta'),
                Param(datatype='double', name='gamma'),
            ],
            java_class_name=None),
        NativeMethod(
            return_type='Throwable',
            static=True,
            name='MessWithJavaException',
            params=[Param(datatype='Throwable', name='e')],
            java_class_name=None)
    ]
    self.AssertListEquals(golden_natives, natives)
    h1 = jni_generator.InlHeaderFileGenerator('', 'org/chromium/TestJni',
                                              natives, [], [], jni_params,
                                              TestOptions())
    self.AssertGoldenTextEquals(h1.GetContent())
    h2 = jni_registration_generator.HeaderGenerator(
        '',
        'org/chromium/TestJni',
        natives,
        jni_params,
        True,
        use_proxy_hash=False)
    content = TestGenerator._MergeRegistrationForTests([h2.Generate()])

    self.AssertGoldenTextEquals(
        jni_registration_generator.CreateFromDict(content, use_hash=False),
        suffix='Registrations')

  def testInnerClassNatives(self):
    test_data = """
    class MyInnerClass {
      @NativeCall("MyInnerClass")
      private native int nativeInit();
    }
    """
    natives = jni_generator.ExtractNatives(test_data, 'int')
    golden_natives = [
        NativeMethod(
            return_type='int',
            static=False,
            name='Init',
            params=[],
            java_class_name='MyInnerClass')
    ]
    self.AssertListEquals(golden_natives, natives)
    jni_params = jni_generator.JniParams('')
    h = jni_generator.InlHeaderFileGenerator('', 'org/chromium/TestJni',
                                             natives, [], [], jni_params,
                                             TestOptions())
    self.AssertGoldenTextEquals(h.GetContent())

  def testInnerClassNativesMultiple(self):
    test_data = """
    class MyInnerClass {
      @NativeCall("MyInnerClass")
      private native int nativeInit();
    }
    class MyOtherInnerClass {
      @NativeCall("MyOtherInnerClass")
      private native int nativeInit();
    }
    """
    natives = jni_generator.ExtractNatives(test_data, 'int')
    golden_natives = [
        NativeMethod(
            return_type='int',
            static=False,
            name='Init',
            params=[],
            java_class_name='MyInnerClass'),
        NativeMethod(
            return_type='int',
            static=False,
            name='Init',
            params=[],
            java_class_name='MyOtherInnerClass')
    ]
    self.AssertListEquals(golden_natives, natives)
    jni_params = jni_generator.JniParams('')
    h = jni_generator.InlHeaderFileGenerator('', 'org/chromium/TestJni',
                                             natives, [], [], jni_params,
                                             TestOptions())
    self.AssertGoldenTextEquals(h.GetContent())

  def testInnerClassNativesBothInnerAndOuter(self):
    test_data = """
    class MyOuterClass {
      private native int nativeInit();
      class MyOtherInnerClass {
        @NativeCall("MyOtherInnerClass")
        private native int nativeInit();
      }
    }
    """
    natives = jni_generator.ExtractNatives(test_data, 'int')
    golden_natives = [
        NativeMethod(
            return_type='int',
            static=False,
            name='Init',
            params=[],
            java_class_name=None),
        NativeMethod(
            return_type='int',
            static=False,
            name='Init',
            params=[],
            java_class_name='MyOtherInnerClass')
    ]
    self.AssertListEquals(golden_natives, natives)
    jni_params = jni_generator.JniParams('')
    h = jni_generator.InlHeaderFileGenerator('', 'org/chromium/TestJni',
                                             natives, [], [], jni_params,
                                             TestOptions())
    self.AssertGoldenTextEquals(h.GetContent())

    h2 = jni_registration_generator.HeaderGenerator(
        '',
        'org/chromium/TestJni',
        natives,
        jni_params,
        True,
        use_proxy_hash=False)
    content = TestGenerator._MergeRegistrationForTests([h2.Generate()])

    self.AssertGoldenTextEquals(
        jni_registration_generator.CreateFromDict(content, use_hash=False),
        suffix='Registrations')

  def testCalledByNatives(self):
    test_data = """"
    import android.graphics.Bitmap;
    import android.view.View;
    import java.io.InputStream;
    import java.util.List;

    class InnerClass {}

    @CalledByNative
    @SomeOtherA
    @SomeOtherB
    public InnerClass showConfirmInfoBar(int nativeInfoBar,
            String buttonOk, String buttonCancel, String title, Bitmap icon) {
        InfoBar infobar = new ConfirmInfoBar(nativeInfoBar, mContext,
                                             buttonOk, buttonCancel,
                                             title, icon);
        return infobar;
    }
    @CalledByNative
    InnerClass showAutoLoginInfoBar(int nativeInfoBar,
            String realm, String account, String args) {
        AutoLoginInfoBar infobar = new AutoLoginInfoBar(nativeInfoBar, mContext,
                realm, account, args);
        if (infobar.displayedAccountCount() == 0)
            infobar = null;
        return infobar;
    }
    @CalledByNative("InfoBar")
    void dismiss();
    @SuppressWarnings("unused")
    @CalledByNative
    private static boolean shouldShowAutoLogin(View view,
            String realm, String account, String args) {
        AccountManagerContainer accountManagerContainer =
            new AccountManagerContainer((Activity)contentView.getContext(),
            realm, account, args);
        String[] logins = accountManagerContainer.getAccountLogins(null);
        return logins.length != 0;
    }
    @CalledByNative
    static InputStream openUrl(String url) {
        return null;
    }
    @CalledByNative
    private void activateHardwareAcceleration(final boolean activated,
            final int iPid, final int iType,
            final int iPrimaryID, final int iSecondaryID) {
      if (!activated) {
          return
      }
    }
    @CalledByNative
    public static @Status int updateStatus(@Status int status) {
        return getAndUpdateStatus(status);
    }
    @CalledByNativeUnchecked
    private void uncheckedCall(int iParam);

    @CalledByNative
    public byte[] returnByteArray();

    @CalledByNative
    public boolean[] returnBooleanArray();

    @CalledByNative
    public char[] returnCharArray();

    @CalledByNative
    public short[] returnShortArray();

    @CalledByNative
    public int[] returnIntArray();

    @CalledByNative
    public long[] returnLongArray();

    @CalledByNative
    public double[] returnDoubleArray();

    @CalledByNative
    public Object[] returnObjectArray();

    @CalledByNative
    public byte[][] returnArrayOfByteArray();

    @CalledByNative
    public Bitmap.CompressFormat getCompressFormat();

    @CalledByNative
    public List<Bitmap.CompressFormat> getCompressFormatList();
    """
    jni_params = jni_generator.JniParams('org/chromium/Foo')
    jni_params.ExtractImportsAndInnerClasses(test_data)
    called_by_natives = jni_generator.ExtractCalledByNatives(
        jni_params, test_data, always_mangle=False)
    golden_called_by_natives = [
        CalledByNative(
            return_type='InnerClass',
            system_class=False,
            static=False,
            name='showConfirmInfoBar',
            method_id_var_name='showConfirmInfoBar',
            java_class_name='',
            params=[
                Param(datatype='int', name='nativeInfoBar'),
                Param(datatype='String', name='buttonOk'),
                Param(datatype='String', name='buttonCancel'),
                Param(datatype='String', name='title'),
                Param(datatype='Bitmap', name='icon')
            ],
            env_call=('Object', ''),
            unchecked=False,
        ),
        CalledByNative(
            return_type='InnerClass',
            system_class=False,
            static=False,
            name='showAutoLoginInfoBar',
            method_id_var_name='showAutoLoginInfoBar',
            java_class_name='',
            params=[
                Param(datatype='int', name='nativeInfoBar'),
                Param(datatype='String', name='realm'),
                Param(datatype='String', name='account'),
                Param(datatype='String', name='args')
            ],
            env_call=('Object', ''),
            unchecked=False,
        ),
        CalledByNative(
            return_type='void',
            system_class=False,
            static=False,
            name='dismiss',
            method_id_var_name='dismiss',
            java_class_name='InfoBar',
            params=[],
            env_call=('Void', ''),
            unchecked=False,
        ),
        CalledByNative(
            return_type='boolean',
            system_class=False,
            static=True,
            name='shouldShowAutoLogin',
            method_id_var_name='shouldShowAutoLogin',
            java_class_name='',
            params=[
                Param(datatype='View', name='view'),
                Param(datatype='String', name='realm'),
                Param(datatype='String', name='account'),
                Param(datatype='String', name='args')
            ],
            env_call=('Boolean', ''),
            unchecked=False,
        ),
        CalledByNative(
            return_type='InputStream',
            system_class=False,
            static=True,
            name='openUrl',
            method_id_var_name='openUrl',
            java_class_name='',
            params=[Param(datatype='String', name='url')],
            env_call=('Object', ''),
            unchecked=False,
        ),
        CalledByNative(
            return_type='void',
            system_class=False,
            static=False,
            name='activateHardwareAcceleration',
            method_id_var_name='activateHardwareAcceleration',
            java_class_name='',
            params=[
                Param(datatype='boolean', name='activated'),
                Param(datatype='int', name='iPid'),
                Param(datatype='int', name='iType'),
                Param(datatype='int', name='iPrimaryID'),
                Param(datatype='int', name='iSecondaryID'),
            ],
            env_call=('Void', ''),
            unchecked=False,
        ),
        CalledByNative(
            return_type='int',
            system_class=False,
            static=True,
            name='updateStatus',
            method_id_var_name='updateStatus',
            java_class_name='',
            params=[
                Param(annotations=['@Status'], datatype='int', name='status')
            ],
            env_call=('Integer', ''),
            unchecked=False,
        ),
        CalledByNative(
            return_type='void',
            system_class=False,
            static=False,
            name='uncheckedCall',
            method_id_var_name='uncheckedCall',
            java_class_name='',
            params=[Param(datatype='int', name='iParam')],
            env_call=('Void', ''),
            unchecked=True,
        ),
        CalledByNative(
            return_type='byte[]',
            system_class=False,
            static=False,
            name='returnByteArray',
            method_id_var_name='returnByteArray',
            java_class_name='',
            params=[],
            env_call=('Void', ''),
            unchecked=False,
        ),
        CalledByNative(
            return_type='boolean[]',
            system_class=False,
            static=False,
            name='returnBooleanArray',
            method_id_var_name='returnBooleanArray',
            java_class_name='',
            params=[],
            env_call=('Void', ''),
            unchecked=False,
        ),
        CalledByNative(
            return_type='char[]',
            system_class=False,
            static=False,
            name='returnCharArray',
            method_id_var_name='returnCharArray',
            java_class_name='',
            params=[],
            env_call=('Void', ''),
            unchecked=False,
        ),
        CalledByNative(
            return_type='short[]',
            system_class=False,
            static=False,
            name='returnShortArray',
            method_id_var_name='returnShortArray',
            java_class_name='',
            params=[],
            env_call=('Void', ''),
            unchecked=False,
        ),
        CalledByNative(
            return_type='int[]',
            system_class=False,
            static=False,
            name='returnIntArray',
            method_id_var_name='returnIntArray',
            java_class_name='',
            params=[],
            env_call=('Void', ''),
            unchecked=False,
        ),
        CalledByNative(
            return_type='long[]',
            system_class=False,
            static=False,
            name='returnLongArray',
            method_id_var_name='returnLongArray',
            java_class_name='',
            params=[],
            env_call=('Void', ''),
            unchecked=False,
        ),
        CalledByNative(
            return_type='double[]',
            system_class=False,
            static=False,
            name='returnDoubleArray',
            method_id_var_name='returnDoubleArray',
            java_class_name='',
            params=[],
            env_call=('Void', ''),
            unchecked=False,
        ),
        CalledByNative(
            return_type='Object[]',
            system_class=False,
            static=False,
            name='returnObjectArray',
            method_id_var_name='returnObjectArray',
            java_class_name='',
            params=[],
            env_call=('Void', ''),
            unchecked=False,
        ),
        CalledByNative(
            return_type='byte[][]',
            system_class=False,
            static=False,
            name='returnArrayOfByteArray',
            method_id_var_name='returnArrayOfByteArray',
            java_class_name='',
            params=[],
            env_call=('Void', ''),
            unchecked=False,
        ),
        CalledByNative(
            return_type='Bitmap.CompressFormat',
            system_class=False,
            static=False,
            name='getCompressFormat',
            method_id_var_name='getCompressFormat',
            java_class_name='',
            params=[],
            env_call=('Void', ''),
            unchecked=False,
        ),
        CalledByNative(
            return_type='List<Bitmap.CompressFormat>',
            system_class=False,
            static=False,
            name='getCompressFormatList',
            method_id_var_name='getCompressFormatList',
            java_class_name='',
            params=[],
            env_call=('Void', ''),
            unchecked=False,
        ),
    ]
    self.AssertListEquals(golden_called_by_natives, called_by_natives)
    h = jni_generator.InlHeaderFileGenerator('', 'org/chromium/TestJni', [],
                                             called_by_natives, [], jni_params,
                                             TestOptions())
    self.AssertGoldenTextEquals(h.GetContent())

  def testCalledByNativeParseError(self):
    try:
      jni_params = jni_generator.JniParams('')
      jni_generator.ExtractCalledByNatives(
          jni_params,
          """
@CalledByNative
public static int foo(); // This one is fine

@CalledByNative
scooby doo
""",
          always_mangle=False)
      self.fail('Expected a ParseError')
    except jni_generator.ParseError as e:
      self.assertEqual(('@CalledByNative', 'scooby doo'), e.context_lines)

  def testFullyQualifiedClassName(self):
    contents = """
// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import org.chromium.base.BuildInfo;
"""
    self.assertEqual(
        'org/chromium/content/browser/Foo',
        jni_generator.ExtractFullyQualifiedJavaClassName(
            'org/chromium/content/browser/Foo.java', contents))
    self.assertEqual(
        'org/chromium/content/browser/Foo',
        jni_generator.ExtractFullyQualifiedJavaClassName(
            'frameworks/Foo.java', contents))
    self.assertRaises(SyntaxError,
                      jni_generator.ExtractFullyQualifiedJavaClassName,
                      'com/foo/Bar', 'no PACKAGE line')

  def testMethodNameMangling(self):
    jni_params = jni_generator.JniParams('')
    self.assertEqual(
        'closeV',
        jni_generator.GetMangledMethodName(jni_params, 'close', [], 'void'))
    self.assertEqual(
        'readI_AB_I_I',
        jni_generator.GetMangledMethodName(jni_params, 'read', [
            Param(name='p1', datatype='byte[]'),
            Param(name='p2', datatype='int'),
            Param(name='p3', datatype='int'),
        ], 'int'))
    self.assertEqual(
        'openJIIS_JLS',
        jni_generator.GetMangledMethodName(jni_params, 'open', [
            Param(name='p1', datatype='java/lang/String'),
        ], 'java/io/InputStream'))

  def testMethodNameAlwaysMangle(self):
    test_data = """
    import f.o.o.Bar;
    import f.o.o.Baz;

    class Clazz {
      @CalledByNative
      public Baz methodz(Bar bar) {
        return null;
      }
    }
    """
    jni_params = jni_generator.JniParams('org/chromium/Foo')
    jni_params.ExtractImportsAndInnerClasses(test_data)
    called_by_natives = jni_generator.ExtractCalledByNatives(
        jni_params, test_data, always_mangle=True)
    self.assertEqual(1, len(called_by_natives))
    method = called_by_natives[0]
    self.assertEqual('methodzFOOB_FOOB', method.method_id_var_name)

  def testFromJavaPGenerics(self):
    contents = """
public abstract class java.util.HashSet<T> extends java.util.AbstractSet<E>
      implements java.util.Set<E>, java.lang.Cloneable, java.io.Serializable {
    public void dummy();
      Signature: ()V
    public java.lang.Class<?> getClass();
      Signature: ()Ljava/lang/Class<*>;
}
"""
    jni_from_javap = jni_generator.JNIFromJavaP(
        contents.split('\n'), TestOptions())
    self.assertEqual(2, len(jni_from_javap.called_by_natives))
    self.AssertGoldenTextEquals(jni_from_javap.GetContent())

  def testSnippnetJavap6_7_8(self):
    content_javap6 = """
public class java.util.HashSet {
public boolean add(java.lang.Object);
 Signature: (Ljava/lang/Object;)Z
}
"""

    content_javap7 = """
public class java.util.HashSet {
public boolean add(E);
  Signature: (Ljava/lang/Object;)Z
}
"""

    content_javap8 = """
public class java.util.HashSet {
  public boolean add(E);
    descriptor: (Ljava/lang/Object;)Z
}
"""

    jni_from_javap6 = jni_generator.JNIFromJavaP(
        content_javap6.split('\n'), TestOptions())
    jni_from_javap7 = jni_generator.JNIFromJavaP(
        content_javap7.split('\n'), TestOptions())
    jni_from_javap8 = jni_generator.JNIFromJavaP(
        content_javap8.split('\n'), TestOptions())
    self.assertTrue(jni_from_javap6.GetContent())
    self.assertTrue(jni_from_javap7.GetContent())
    self.assertTrue(jni_from_javap8.GetContent())
    # Ensure the javap7 is correctly parsed and uses the Signature field rather
    # than the "E" parameter.
    self.AssertTextEquals(jni_from_javap6.GetContent(),
                          jni_from_javap7.GetContent())
    # Ensure the javap8 is correctly parsed and uses the descriptor field.
    self.AssertTextEquals(jni_from_javap7.GetContent(),
                          jni_from_javap8.GetContent())

  def testFromJavaP(self):
    contents = self._ReadGoldenFile('testInputStream.javap')
    jni_from_javap = jni_generator.JNIFromJavaP(
        contents.split('\n'), TestOptions())
    self.assertEqual(10, len(jni_from_javap.called_by_natives))
    self.AssertGoldenTextEquals(jni_from_javap.GetContent())

  def testConstantsFromJavaP(self):
    for f in ['testMotionEvent.javap', 'testMotionEvent.javap7']:
      contents = self._ReadGoldenFile(f)
      jni_from_javap = jni_generator.JNIFromJavaP(
          contents.split('\n'), TestOptions())
      self.assertEqual(86, len(jni_from_javap.called_by_natives))
      self.AssertGoldenTextEquals(jni_from_javap.GetContent())

  def testREForNatives(self):
    # We should not match "native SyncSetupFlow" inside the comment.
    test_data = """
    /**
     * Invoked when the setup process is complete so we can disconnect from the
     * private native void nativeSyncSetupFlowHandler();.
     */
    public void destroy() {
        Log.v(TAG, "Destroying native SyncSetupFlow");
        if (mNativeSyncSetupFlow != 0) {
            nativeSyncSetupEnded(mNativeSyncSetupFlow);
            mNativeSyncSetupFlow = 0;
        }
    }
    private native void nativeSyncSetupEnded(
        int nativeAndroidSyncSetupFlowHandler);
    """
    jni_from_java = jni_generator.JNIFromJavaSource(
        test_data, 'foo/bar', TestOptions())
    self.AssertGoldenTextEquals(jni_from_java.GetContent())

  def testRaisesOnNonJNIMethod(self):
    test_data = """
    class MyInnerClass {
      private int Foo(int p0) {
      }
    }
    """
    self.assertRaises(SyntaxError, jni_generator.JNIFromJavaSource, test_data,
                      'foo/bar', TestOptions())

  def testJniSelfDocumentingExample(self):
    generated_text = self._CreateJniHeaderFromFile(
        os.path.join(_JAVA_SRC_DIR, 'SampleForTests.java'),
        'org/chromium/example/jni_generator/SampleForTests')
    self.AssertGoldenTextEquals(
        generated_text, golden_file='SampleForTests_jni.golden')

  def testNoWrappingPreprocessorLines(self):
    test_data = """
    package com.google.lookhowextremelylongiam.snarf.icankeepthisupallday;

    class ReallyLongClassNamesAreAllTheRage {
        private static native int nativeTest();
    }
    """
    jni_from_java = jni_generator.JNIFromJavaSource(
        test_data, ('com/google/lookhowextremelylongiam/snarf/'
                    'icankeepthisupallday/ReallyLongClassNamesAreAllTheRage'),
        TestOptions())
    jni_lines = jni_from_java.GetContent().split('\n')
    line = next(
        line for line in jni_lines if line.lstrip().startswith('#ifndef'))
    self.assertTrue(
        len(line) > 80, ('Expected #ifndef line to be > 80 chars: ', line))

  def testImports(self):
    import_header = """
// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.app;

import android.app.Service;
import android.content.Context;
import android.content.Intent;
import android.graphics.SurfaceTexture;
import android.os.Bundle;
import android.os.IBinder;
import android.os.ParcelFileDescriptor;
import android.os.Process;
import android.os.RemoteException;
import android.util.Log;
import android.view.Surface;

import java.util.ArrayList;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.content.app.ContentMain;
import org.chromium.content.browser.SandboxedProcessConnection;
import org.chromium.content.common.ISandboxedProcessCallback;
import org.chromium.content.common.ISandboxedProcessService;
import org.chromium.content.common.WillNotRaise.AnException;
import org.chromium.content.common.WillRaise.AnException;

import static org.chromium.Bar.Zoo;

class Foo {
  public static class BookmarkNode implements Parcelable {
  }
  public interface PasswordListObserver {
  }
}
    """
    jni_params = jni_generator.JniParams('org/chromium/content/app/Foo')
    jni_params.ExtractImportsAndInnerClasses(import_header)
    self.assertTrue('Lorg/chromium/content/common/ISandboxedProcessService' in
                    jni_params._imports)
    self.assertTrue('Lorg/chromium/Bar/Zoo' in jni_params._imports)
    self.assertTrue('Lorg/chromium/content/app/Foo$BookmarkNode' in jni_params.
                    _inner_classes)
    self.assertTrue('Lorg/chromium/content/app/Foo$PasswordListObserver' in
                    jni_params._inner_classes)
    self.assertEqual('Lorg/chromium/content/app/ContentMain$Inner;',
                     jni_params.JavaToJni('ContentMain.Inner'))
    self.assertRaises(SyntaxError, jni_params.JavaToJni, 'AnException')

  def testJniParamsJavaToJni(self):
    jni_params = jni_generator.JniParams('')
    self.AssertTextEquals('I', jni_params.JavaToJni('int'))
    self.AssertTextEquals('[B', jni_params.JavaToJni('byte[]'))
    self.AssertTextEquals('[Ljava/nio/ByteBuffer;',
                          jni_params.JavaToJni('java/nio/ByteBuffer[]'))

  def testNativesLong(self):
    test_options = TestOptions()
    test_options.ptr_type = 'long'
    test_data = """"
    private native void nativeDestroy(long nativeChromeBrowserProvider);
    """
    jni_params = jni_generator.JniParams('')
    jni_params.ExtractImportsAndInnerClasses(test_data)
    natives = jni_generator.ExtractNatives(test_data, test_options.ptr_type)
    golden_natives = [
        NativeMethod(
            return_type='void',
            static=False,
            name='Destroy',
            params=[Param(datatype='long', name='nativeChromeBrowserProvider')],
            java_class_name=None,
            ptr_type=test_options.ptr_type),
    ]
    self.AssertListEquals(golden_natives, natives)
    h = jni_generator.InlHeaderFileGenerator(
        '', 'org/chromium/TestJni', natives, [], [], jni_params, test_options)
    self.AssertGoldenTextEquals(h.GetContent())

  def testMainDexAnnotation(self):
    mainDexEntries = [
        '@MainDex public class Test {',
        '@MainDex public class Test{',
        """@MainDex
         public class Test {
      """,
        """@MainDex public class Test
         {
      """,
        '@MainDex /* This class is a test */ public class Test {',
        '@MainDex public class Test implements java.io.Serializable {',
        '@MainDex public class Test implements java.io.Serializable, Bidule {',
        '@MainDex public class Test extends BaseTest {',
        """@MainDex
         public class Test extends BaseTest implements Bidule {
      """,
        """@MainDex
         public class Test extends BaseTest implements Bidule, Machin, Chose {
      """,
        """@MainDex
         public class Test implements Testable<java.io.Serializable> {
      """,
        '@MainDex public class Test implements Testable<java.io.Serializable> '
        ' {',
        '@a.B @MainDex @C public class Test extends Testable<Serializable> {',
        """public class Test extends Testable<java.io.Serializable> {
         @MainDex void func() {}
      """,
    ]
    for entry in mainDexEntries:
      self.assertEqual(True, IsMainDexJavaClass(entry), entry)

  def testNoMainDexAnnotation(self):
    noMainDexEntries = [
        'public class Test {', '@NotMainDex public class Test {',
        '// @MainDex public class Test {', '/* @MainDex */ public class Test {',
        'public class Test implements java.io.Serializable {',
        '@MainDexNot public class Test {',
        'public class Test extends BaseTest {'
    ]
    for entry in noMainDexEntries:
      self.assertEqual(False, IsMainDexJavaClass(entry))

  def testNativeExportsOnlyOption(self):
    test_data = """
    package org.chromium.example.jni_generator;

    /** The pointer to the native Test. */
    long nativeTest;

    class Test {
        private static native int nativeStaticMethod(long nativeTest, int arg1);
        private native int nativeMethod(long nativeTest, int arg1);
        @CalledByNative
        private void testMethodWithParam(int iParam);
        @CalledByNative
        private String testMethodWithParamAndReturn(int iParam);
        @CalledByNative
        private static int testStaticMethodWithParam(int iParam);
        @CalledByNative
        private static double testMethodWithNoParam();
        @CalledByNative
        private static String testStaticMethodWithNoParam();

        class MyInnerClass {
          @NativeCall("MyInnerClass")
          private native int nativeInit();
        }
        class MyOtherInnerClass {
          @NativeCall("MyOtherInnerClass")
          private native int nativeInit();
        }
    }
    """
    options = TestOptions()
    options.native_exports_optional = False
    jni_from_java = jni_generator.JNIFromJavaSource(
        test_data, 'org/chromium/example/jni_generator/SampleForTests', options)
    self.AssertGoldenTextEquals(jni_from_java.GetContent())

  def testOuterInnerRaises(self):
    test_data = """
    package org.chromium.media;

    @CalledByNative
    static int getCaptureFormatWidth(VideoCapture.CaptureFormat format) {
        return format.getWidth();
    }
    """

    def willRaise():
      jni_generator.JNIFromJavaSource(test_data,
                                      'org/chromium/media/VideoCaptureFactory',
                                      TestOptions())

    self.assertRaises(SyntaxError, willRaise)

  def testSingleJNIAdditionalImport(self):
    test_data = """
    package org.chromium.foo;

    @JNIAdditionalImport(Bar.class)
    class Foo {

    @CalledByNative
    private static void calledByNative(Bar.Callback callback) {
    }

    private static native void nativeDoSomething(Bar.Callback callback);
    }
    """
    jni_from_java = jni_generator.JNIFromJavaSource(test_data,
                                                    'org/chromium/foo/Foo',
                                                    TestOptions())
    self.AssertGoldenTextEquals(jni_from_java.GetContent())

  def testMultipleJNIAdditionalImport(self):
    test_data = """
    package org.chromium.foo;

    @JNIAdditionalImport({Bar1.class, Bar2.class})
    class Foo {

    @CalledByNative
    private static void calledByNative(Bar1.Callback callback1,
                                       Bar2.Callback callback2) {
    }

    private static native void nativeDoSomething(Bar1.Callback callback1,
                                                 Bar2.Callback callback2);
    }
    """
    jni_from_java = jni_generator.JNIFromJavaSource(test_data,
                                                    'org/chromium/foo/Foo',
                                                    TestOptions())
    self.AssertGoldenTextEquals(jni_from_java.GetContent())

  def testTracing(self):
    test_data = """
    package org.chromium.foo;

    @JNINamespace("org::chromium_foo")
    class Foo {

    @CalledByNative
    Foo();

    @CalledByNative
    void callbackFromNative();

    native void nativeInstanceMethod(long nativeInstance);

    static native void nativeStaticMethod();
    }
    """
    options_with_tracing = TestOptions()
    options_with_tracing.enable_tracing = True
    jni_from_java = jni_generator.JNIFromJavaSource(
        test_data, 'org/chromium/foo/Foo', options_with_tracing)
    self.AssertGoldenTextEquals(jni_from_java.GetContent())

  def testStaticBindingCaller(self):
    test_data = """
    package org.chromium.foo;

    class Bar {
      static native void nativeShouldBindCaller(Object caller);
      static native void nativeShouldBindCaller(Object caller, int a);
      static native void nativeFoo(long nativeNativeObject, Bar caller);
      static native void nativeFoo(long nativeNativeObject, Bar caller, int a);
      native void nativeCallNativeMethod(long nativePtr);
      @NativeClassQualifiedName("Foo::Bar")
      native void nativeCallWithQualifiedObject(long nativePtr);
    }
    """

    jni_from_java = jni_generator.JNIFromJavaSource(test_data,
                                                    'org/chromium/foo/Foo',
                                                    TestOptions())
    self.AssertGoldenTextEquals(jni_from_java.GetContent())


class ProxyTestGenerator(BaseTest):

  def _BuildRegDictFromSample(self, options=None):
    if options is None:
      options = TestOptions()

    path = self._JoinScriptDir(
        os.path.join(_JAVA_SRC_DIR, 'SampleForAnnotationProcessor.java'))
    reg_dict = jni_registration_generator._DictForPath(path)
    reg_dict = self._MergeRegistrationForTests([reg_dict])

    return reg_dict

  def testProxyNativesWithNatives(self):
    test_data = """
    package org.chromium.foo;

    class Foo {

    @NativeMethods
    interface Natives {
       void foo();
       String bar(String s, int y, char x, short z);
       String[] foobar(String[] a);
       void baz(long nativePtr, BazClass caller);
       void fooBar(long nativePtr);
    }

    void justARegularFunction();

    native void nativeInstanceMethod(long nativeInstance);
    static native void nativeStaticMethod();

    }
    """
    options_with_tracing = TestOptions()
    options_with_tracing.enable_tracing = True
    jni_from_java = jni_generator.JNIFromJavaSource(
        test_data, 'org/chromium/foo/Foo', options_with_tracing)
    self.AssertGoldenTextEquals(jni_from_java.GetContent())

  def testEscapingProxyNatives(self):
    test_data = """
    class SampleProxyJni {
      @NativeMethods
      interface Natives {
        void foo_bar();
        void foo__bar();
      }
    }
    """
    qualified_clazz = 'org/chromium/example/SampleProxyJni'

    natives = jni_generator.ProxyHelpers.ExtractStaticProxyNatives(
        qualified_clazz, test_data, 'long')

    golden_natives = [
        NativeMethod(
            return_type='void',
            static=True,
            name='foo_bar',
            params=[],
            java_class_name=None,
            is_proxy=True,
            proxy_name='org_chromium_example_SampleProxyJni_foo_1bar'),
        NativeMethod(
            return_type='void',
            static=True,
            name='foo__bar',
            params=[],
            java_class_name=None,
            is_proxy=True,
            proxy_name='org_chromium_example_SampleProxyJni_foo_1_1bar'),
    ]

    self.AssertListEquals(natives, golden_natives)

  def testProxyNativesMainDex(self):
    test_data = """
    @MainDex
    class Foo() {
      @NativeMethods
      interface Natives {
        void thisismaindex();
      }
      void dontmatchme();
      public static void metoo();
      public static native void this_is_a_non_proxy_native();
    }
    """

    non_main_dex_test_data = """
    class Bar() {
      @NativeMethods
      interface Natives {
        void foo();
        void bar();
      }
    }
    """
    qualified_clazz = 'test/foo/Foo'
    jni_params = TestOptions()

    natives = jni_generator.ProxyHelpers.ExtractStaticProxyNatives(
        qualified_clazz, test_data, 'long')

    golden_natives = [
        NativeMethod(
            return_type='void',
            static=True,
            name='thisismaindex',
            params=[],
            java_class_name=None,
            is_proxy=True,
            proxy_name='test_foo_Foo_thisismaindex'),
    ]

    self.AssertListEquals(natives, golden_natives)

    jni_params = jni_generator.JniParams(qualified_clazz)
    main_dex_header = jni_registration_generator.HeaderGenerator(
        '',
        qualified_clazz,
        natives,
        jni_params,
        main_dex=True,
        use_proxy_hash=False).Generate()
    content = TestGenerator._MergeRegistrationForTests([main_dex_header])

    self.AssertGoldenTextEquals(
        jni_registration_generator.CreateFromDict(content, use_hash=False))

    other_qualified_clazz = 'test/foo/Bar'
    other_natives = jni_generator.ProxyHelpers.ExtractStaticProxyNatives(
        other_qualified_clazz, non_main_dex_test_data, 'long')

    jni_params = jni_generator.JniParams(other_qualified_clazz)
    non_main_dex_header = jni_registration_generator.HeaderGenerator(
        '',
        other_qualified_clazz,
        other_natives,
        jni_params,
        main_dex=False,
        use_proxy_hash=False).Generate()

    content = TestGenerator._MergeRegistrationForTests([main_dex_header] +
                                                       [non_main_dex_header])

    self.AssertGoldenTextEquals(
        jni_registration_generator.CreateFromDict(content, use_hash=False),
        'AndNonMainDex')

  def testProxyNatives(self):
    test_data = """
    class SampleProxyJni {
      private void do_not_match();
      @VisibleForTesting
      @NativeMethods
      @Generated("Test")
      interface Natives {
        @NativeClassQualifiedName("FooAndroid::BarDelegate")
        void foo(long nativePtr);
        int bar(int x, int y);
        String foobar(String x, String y);
      }
      void dontmatchme();
      public static void metoo();
      public static native void this_is_a_non_proxy_native();
    }
    """

    bad_spaced_test_data = """
    class SampleProxyJni{
      @NativeMethods interface
      Natives


      { @NativeClassQualifiedName("FooAndroid::BarDelegate") void
    foo(long nativePtr);
      int              bar(int
      x,  int y); String
        foobar(String x, String y);
      }

    }
    """

    qualified_clazz = 'org/chromium/example/SampleProxyJni'

    natives = jni_generator.ProxyHelpers.ExtractStaticProxyNatives(
        qualified_clazz, test_data, 'long')
    bad_spacing_natives = jni_generator.ProxyHelpers \
      .ExtractStaticProxyNatives(qualified_clazz, bad_spaced_test_data, 'long')
    golden_natives = [
        NativeMethod(
            return_type='void',
            static=True,
            name='foo',
            native_class_name='FooAndroid::BarDelegate',
            params=[Param(datatype='long', name='nativePtr')],
            java_class_name=None,
            is_proxy=True,
            proxy_name='org_chromium_example_SampleProxyJni_foo',
            ptr_type='long'),
        NativeMethod(
            return_type='int',
            static=True,
            name='bar',
            params=[
                Param(datatype='int', name='x'),
                Param(datatype='int', name='y')
            ],
            java_class_name=None,
            is_proxy=True,
            proxy_name='org_chromium_example_SampleProxyJni_bar'),
        NativeMethod(
            return_type='String',
            static=True,
            name='foobar',
            params=[
                Param(datatype='String', name='x'),
                Param(datatype='String', name='y')
            ],
            java_class_name=None,
            is_proxy=True,
            proxy_name='org_chromium_example_SampleProxyJni_foobar'),
    ]
    self.AssertListEquals(golden_natives, natives)
    self.AssertListEquals(golden_natives, bad_spacing_natives)

    jni_params = jni_generator.JniParams(qualified_clazz)
    h1 = jni_generator.InlHeaderFileGenerator('', qualified_clazz, natives, [],
                                              [], jni_params, TestOptions())
    self.AssertGoldenTextEquals(h1.GetContent())
    h2 = jni_registration_generator.HeaderGenerator(
        '', qualified_clazz, natives, jni_params, False, use_proxy_hash=False)
    content = TestGenerator._MergeRegistrationForTests([h2.Generate()])

    proxy_opts = jni_registration_generator.ProxyOptions()
    self.AssertGoldenTextEquals(
        jni_registration_generator.CreateProxyJavaFromDict(content, proxy_opts),
        suffix='Java')

    self.AssertGoldenTextEquals(
        jni_registration_generator.CreateFromDict(content, proxy_opts.use_hash),
        suffix='Registrations')

  def testProxyHashedExample(self):
    opts = TestOptions()
    opts.use_proxy_hash = True
    path = os.path.join(_JAVA_SRC_DIR, 'SampleForAnnotationProcessor.java')

    generated_text = self._CreateJniHeaderFromFile(
        path, 'org/chromium/example/jni_generator/SampleForAnnotationProcessor',
        opts)
    self.AssertGoldenTextEquals(
        generated_text,
        golden_file='HashedSampleForAnnotationProcessor_jni.golden')

    reg_dict = jni_registration_generator._DictForPath(
        self._JoinScriptDir(path))
    reg_dict = self._MergeRegistrationForTests([reg_dict])

    proxy_opts = jni_registration_generator.ProxyOptions()
    self.AssertGoldenTextEquals(
        jni_registration_generator.CreateProxyJavaFromDict(
            reg_dict, proxy_opts),
        golden_file='HashedSampleForAnnotationProcessorGenJni.golden')

  def testProxyJniExample(self):
    generated_text = self._CreateJniHeaderFromFile(
        os.path.join(_JAVA_SRC_DIR, 'SampleForAnnotationProcessor.java'),
        'org/chromium/example/jni_generator/SampleForAnnotationProcessor')
    self.AssertGoldenTextEquals(
        generated_text, golden_file='SampleForAnnotationProcessor_jni.golden')

  def testGenJniFlags(self):
    reg_dict = self._BuildRegDictFromSample()
    proxy_options = jni_registration_generator.ProxyOptions()
    content = jni_registration_generator.CreateProxyJavaFromDict(
        reg_dict, proxy_options)
    self.AssertGoldenTextEquals(content, 'Disabled')

    proxy_options = jni_registration_generator.ProxyOptions(enable_mocks=True)
    content = jni_registration_generator.CreateProxyJavaFromDict(
        reg_dict, proxy_options)
    self.AssertGoldenTextEquals(content, 'MocksEnabled')

    proxy_options = jni_registration_generator.ProxyOptions(
        enable_mocks=True, require_mocks=True)
    content = jni_registration_generator.CreateProxyJavaFromDict(
        reg_dict, proxy_options)
    self.AssertGoldenTextEquals(content, 'MocksRequired')

  def testProxyTypeInfoPreserved(self):
    test_data = """
    package org.chromium.foo;

    class Foo {

    @NativeMethods
    interface Natives {
      char[][] fooProxy(byte[][] b);
      SomeJavaType[][] barProxy(String[][] s, short z);
      String[] foobarProxy(String[] a, int[][] b);
      byte[][] bazProxy(long nativePtr, BazClass caller,
          SomeJavaType[][] someObjects);
    }
    """
    natives = ProxyHelpers.ExtractStaticProxyNatives('org/chromium/foo/FooJni',
                                                     test_data, 'long')
    golden_natives = [
        NativeMethod(
            static=True,
            java_class_name=None,
            return_type='char[][]',
            name='fooProxy',
            params=[Param(datatype='byte[][]', name='b')],
            is_proxy=True,
            proxy_name='org_chromium_foo_FooJni_fooProxy'),
        NativeMethod(
            static=True,
            java_class_name=None,
            return_type='Object[][]',
            name='barProxy',
            params=[
                Param(datatype='String[][]', name='s'),
                Param(datatype='short', name='z')
            ],
            is_proxy=True,
            proxy_name='org_chromium_foo_FooJni_barProxy'),
        NativeMethod(
            static=True,
            java_class_name=None,
            return_type='String[]',
            name='foobarProxy',
            params=[
                Param(datatype='String[]', name='a'),
                Param(datatype='int[][]', name='b')
            ],
            is_proxy=True,
            proxy_name='org_chromium_foo_FooJni_foobarProxy'),
        NativeMethod(
            static=True,
            java_class_name=None,
            return_type='byte[][]',
            name='bazProxy',
            params=[
                Param(datatype='long', name='nativePtr'),
                Param(datatype='Object', name='caller'),
                Param(datatype='Object[][]', name='someObjects')
            ],
            is_proxy=True,
            proxy_name='org_chromium_foo_FooJni_bazProxy',
            ptr_type='long')
    ]
    self.AssertListEquals(golden_natives, natives)


def TouchStamp(stamp_path):
  dir_name = os.path.dirname(stamp_path)
  if not os.path.isdir(dir_name):
    os.makedirs(dir_name)

  with open(stamp_path, 'a'):
    os.utime(stamp_path, None)


def main(argv):
  parser = optparse.OptionParser()
  parser.add_option('--stamp', help='Path to touch on success.')
  parser.add_option(
      '-v', '--verbose', action='store_true', help='Whether to output details.')
  options, _ = parser.parse_args(argv[1:])

  test_result = unittest.main(
      argv=argv[0:1], exit=False, verbosity=(2 if options.verbose else 1))

  if test_result.result.wasSuccessful() and options.stamp:
    TouchStamp(options.stamp)

  return not test_result.result.wasSuccessful()


if __name__ == '__main__':
  sys.exit(main(sys.argv))
