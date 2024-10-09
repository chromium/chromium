# Copyright 2012 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Top-level presubmit script for Chromium.

See https://www.chromium.org/developers/how-tos/depottools/presubmit-scripts/
for more details about the presubmit API built into depot_tools.
"""

from typing import Callable
from typing import Optional
from typing import Sequence
from typing import Tuple
from dataclasses import dataclass

PRESUBMIT_VERSION = '2.0.0'


_EXCLUDED_PATHS = (
    # Generated file
    (r"chrome/android/webapk/shell_apk/src/org/chromium"
     r"/webapk/lib/runtime_library/IWebApkApi.java"),
    # File needs to write to stdout to emulate a tool it's replacing.
    r"chrome/updater/mac/keystone/ksadmin.mm",
    # Generated file.
    (r"^components/variations/proto/devtools/"
     r"client_variations.js"),
    # These are video files, not typescript.
    r"^media/test/data/.*.ts",
    r"^native_client_sdksrc/build_tools/make_rules.py",
    r"^native_client_sdk/src/build_tools/make_simple.py",
    r"^native_client_sdk/src/tools/.*.mk",
    r"^net/tools/spdyshark/.*",
    r"^skia/.*",
    r"^third_party/blink/.*",
    r"^third_party/breakpad/.*",
    # sqlite is an imported third party dependency.
    r"^third_party/sqlite/.*",
    r"^v8/.*",
    r".*MakeFile$",
    r".+_autogen\.h$",
    r".+_pb2(_grpc)?\.py$",
    r".+/pnacl_shim\.c$",
    r"^gpu/config/.*_list_json\.cc$",
    r"tools/md_browser/.*\.css$",
    # Test pages for Maps telemetry tests.
    r"tools/perf/page_sets/maps_perf_test.*",
    # Test pages for WebRTC telemetry tests.
    r"tools/perf/page_sets/webrtc_cases.*",
    # Test file compared with generated output.
    r"tools/polymer/tests/html_to_wrapper/.*.html.ts$",
    # Third-party dependency frozen at a fixed version.
    r"chrome/test/data/webui/chromeos/chai_v4.js$",
)

_EXCLUDED_SET_NO_PARENT_PATHS = (
    # It's for historical reasons that blink isn't a top level directory, where
    # it would be allowed to have "set noparent" to avoid top level owners
    # accidentally +1ing changes.
    'third_party/blink/OWNERS',
)


# Fragment of a regular expression that matches C++ and Objective-C++
# implementation files.
_IMPLEMENTATION_EXTENSIONS = r'\.(cc|cpp|cxx|mm)$'


# Fragment of a regular expression that matches C++ and Objective-C++
# header files.
_HEADER_EXTENSIONS = r'\.(h|hpp|hxx)$'


# Paths with sources that don't use //base.
_NON_BASE_DEPENDENT_PATHS = (
    r"^chrome/browser/browser_switcher/bho/",
    r"^tools/win/",
)


# Regular expression that matches code only used for test binaries
# (best effort).
_TEST_CODE_EXCLUDED_PATHS = (
    r'.*/(fake_|test_|mock_).+%s' % _IMPLEMENTATION_EXTENSIONS,
    # Test support files, like:
    # foo_test_support.cc
    # bar_test_util_linux.cc (suffix)
    # baz_test_base.cc
    r'.+_test_(base|support|util)(_[a-z]+)?%s' % _IMPLEMENTATION_EXTENSIONS,
    # Test suite files, like:
    # foo_browsertest.cc
    # bar_unittest_mac.cc (suffix)
    # baz_unittests.cc (plural)
    r'.+_(api|browser|eg|int|perf|pixel|unit|ui)?test(s)?(_[a-z]+)?%s' %
        _IMPLEMENTATION_EXTENSIONS,
    r'.+_(fuzz|fuzzer)(_[a-z]+)?%s' % _IMPLEMENTATION_EXTENSIONS,
    r'.+sync_service_impl_harness%s' % _IMPLEMENTATION_EXTENSIONS,
    r'.*/(test|tool(s)?)/.*',
    # content_shell is used for running content_browsertests.
    r'content/shell/.*',
    # Web test harness.
    r'content/web_test/.*',
    # Non-production example code.
    r'mojo/examples/.*',
    # Launcher for running iOS tests on the simulator.
    r'testing/iossim/iossim\.mm$',
    # EarlGrey app side code for tests.
    r'ios/.*_app_interface\.mm$',
    # Views Examples code
    r'ui/views/examples/.*',
    # Chromium Codelab
    r'codelabs/*'
)

_THIRD_PARTY_EXCEPT_BLINK = 'third_party/(?!blink/)'

_TEST_ONLY_WARNING = (
    'You might be calling functions intended only for testing from\n'
    'production code.  If you are doing this from inside another method\n'
    'named as *ForTesting(), then consider exposing things to have tests\n'
    'make that same call directly.\n'
    'If that is not possible, you may put a comment on the same line with\n'
    '  // IN-TEST \n'
    'to tell the PRESUBMIT script that the code is inside a *ForTesting()\n'
    'method and can be ignored. Do not do this inside production code.\n'
    'The android-binary-size trybot will block if the method exists in the\n'
    'release apk.\n'
    'Note: this warning might be a false positive (crbug.com/1196548).')


@dataclass
class BanRule:
    # String pattern. If the pattern begins with a slash, the pattern will be
    # treated as a regular expression instead.
    pattern: str
    # Explanation as a sequence of strings. Each string in the sequence will be
    # printed on its own line.
    explanation: Tuple[str, ...]
    # Whether or not to treat this ban as a fatal error. If unspecified,
    # defaults to true.
    treat_as_error: Optional[bool] = None
    # Paths that should be excluded from the ban check. Each string is a regular
    # expression that will be matched against the path of the file being checked
    # relative to the root of the source tree.
    excluded_paths: Optional[Sequence[str]] = None


_BANNED_JAVA_IMPORTS : Sequence[BanRule] = (
    BanRule(
      'import java.net.URI;',
      (
       'Use org.chromium.url.GURL instead of java.net.URI, where possible.',
      ),
      excluded_paths=(
        (r'net/android/javatests/src/org/chromium/net/'
         'AndroidProxySelectorTest\.java'),
        r'components/cronet/',
        r'third_party/robolectric/local/',
      ),
    ),
    BanRule(
      'import android.annotation.TargetApi;',
      (
       'Do not use TargetApi, use @androidx.annotation.RequiresApi instead. '
       'RequiresApi ensures that any calls are guarded by the appropriate '
       'SDK_INT check. See https://crbug.com/1116486.',
      ),
    ),
    BanRule(
      'import androidx.test.rule.UiThreadTestRule;',
      (
       'Do not use UiThreadTestRule, just use '
       '@org.chromium.base.test.UiThreadTest on test methods that should run '
       'on the UI thread. See https://crbug.com/1111893.',
      ),
    ),
    BanRule(
      'import androidx.test.annotation.UiThreadTest;',
      ('Do not use androidx.test.annotation.UiThreadTest, use '
       'org.chromium.base.test.UiThreadTest instead. See '
       'https://crbug.com/1111893.',
      ),
    ),
    BanRule(
      'import androidx.test.rule.ActivityTestRule;',
      (
       'Do not use ActivityTestRule, use '
       'org.chromium.base.test.BaseActivityTestRule instead.',
      ),
      excluded_paths=(
        'components/cronet/',
      ),
    ),
    BanRule(
      'import androidx.vectordrawable.graphics.drawable.VectorDrawableCompat;',
      (
       'Do not use VectorDrawableCompat, use getResources().getDrawable() to '
       'avoid extra indirections. Please also add trace event as the call '
       'might take more than 20 ms to complete.',
      ),
    ),
)

_BANNED_JAVA_FUNCTIONS : Sequence[BanRule] = (
    BanRule(
      'StrictMode.allowThreadDiskReads()',
      (
       'Prefer using StrictModeContext.allowDiskReads() to using StrictMode '
       'directly.',
      ),
      False,
    ),
    BanRule(
      'StrictMode.allowThreadDiskWrites()',
      (
       'Prefer using StrictModeContext.allowDiskWrites() to using StrictMode '
       'directly.',
      ),
      False,
    ),
    BanRule(
      '.waitForIdleSync()',
      (
       'Do not use waitForIdleSync as it masks underlying issues. There is '
       'almost always something else you should wait on instead.',
      ),
      False,
    ),
    BanRule(
      r'/(?<!\bsuper\.)(?<!\bIntent )\bregisterReceiver\(',
      (
       'Do not call android.content.Context.registerReceiver (or an override) '
       'directly. Use one of the wrapper methods defined in '
       'org.chromium.base.ContextUtils, such as '
       'registerProtectedBroadcastReceiver, '
       'registerExportedBroadcastReceiver, or '
       'registerNonExportedBroadcastReceiver. See their documentation for '
       'which one to use.',
      ),
      True,
      excluded_paths=(
          r'.*Test[^a-z]',
          r'third_party/',
          'base/android/java/src/org/chromium/base/ContextUtils.java',
          'chromecast/browser/android/apk/src/org/chromium/chromecast/shell/BroadcastReceiverScope.java',
      ),
    ),
    BanRule(
      r'/(?:extends|new)\s*(?:android.util.)?Property<[A-Za-z.]+,\s*(?:Integer|Float)>',
      (
       'Do not use Property<..., Integer|Float>, but use FloatProperty or '
       'IntProperty because it will avoid unnecessary autoboxing of '
       'primitives.',
      ),
    ),
    BanRule(
      'requestLayout()',
      (
          'Layouts can be expensive. Prefer using ViewUtils.requestLayout(), '
          'which emits a trace event with additional information to help with '
          'scroll jank investigations. See http://crbug.com/1354176.',
      ),
      False,
      excluded_paths=(
          'ui/android/java/src/org/chromium/ui/base/ViewUtils.java',
      ),
    ),
    BanRule(
      'ProfileManager.getLastUsedRegularProfile()',
      (
       'Prefer passing in the Profile reference instead of relying on the '
       'static getLastUsedRegularProfile() call. Only top level entry points '
       '(e.g. Activities) should call this method. Otherwise, the Profile '
       'should either be passed in explicitly or retreived from an existing '
       'entity with a reference to the Profile (e.g. WebContents).',
      ),
      False,
      excluded_paths=(
        r'.*Test[A-Z]?.*\.java',
      ),
    ),
    BanRule(
      r'/(ResourcesCompat|getResources\(\))\.getDrawable\(\)',
      (
       'getDrawable() can be expensive. If you have a lot of calls to '
       'GetDrawable() or your code may introduce janks, please put your calls '
       'inside a trace().',
      ),
      False,
      excluded_paths=(
        r'.*Test[A-Z]?.*\.java',
      ),
    ),
    BanRule(
      r'/RecordHistogram\.getHistogram(ValueCount|TotalCount|Samples)ForTesting\(',
      (
       'Raw histogram counts are easy to misuse; for example they don\'t reset '
       'between batched tests. Use HistogramWatcher to check histogram records '
       'instead.',
      ),
      False,
      excluded_paths=(
        'base/android/javatests/src/org/chromium/base/metrics/RecordHistogramTest.java',
        'base/test/android/javatests/src/org/chromium/base/test/util/HistogramWatcher.java',
      ),
    ),
)

_BANNED_JAVASCRIPT_FUNCTIONS : Sequence [BanRule] = (
    BanRule(
      r'/\bchrome\.send\b',
      (
       'The use of chrome.send is disallowed in Chrome (context: https://chromium.googlesource.com/chromium/src/+/refs/heads/main/docs/security/handling-messages-from-web-content.md).',
       'Please use mojo instead for new webuis. https://docs.google.com/document/d/1RF-GSUoveYa37eoyZ9EhwMtaIwoW7Z88pIgNZ9YzQi4/edit#heading=h.gkk22wgk6wff',
      ),
      True,
      (
          r'^(?!ash\/webui).+',
          # TODO(crbug.com/1385601): pre-existing violations still need to be
          # cleaned up.
          'ash/webui/common/resources/cr.m.js',
          'ash/webui/common/resources/multidevice_setup/multidevice_setup_browser_proxy.js',
          'ash/webui/common/resources/quick_unlock/lock_screen_constants.ts',
          'ash/webui/common/resources/smb_shares/smb_browser_proxy.js',
          'ash/webui/connectivity_diagnostics/resources/connectivity_diagnostics.ts',
          'ash/webui/diagnostics_ui/resources/diagnostics_browser_proxy.ts',
          'ash/webui/multidevice_debug/resources/logs.js',
          'ash/webui/multidevice_debug/resources/webui.js',
          'ash/webui/projector_app/resources/annotator/trusted/annotator_browser_proxy.js',
          'ash/webui/projector_app/resources/app/trusted/projector_browser_proxy.js',
          # TODO(b/301634378): Remove violation exception once Scanning App
          # migrated off usage of `chrome.send`.
          'ash/webui/scanning/resources/scanning_browser_proxy.ts',
      ),
    ),
)

_BANNED_OBJC_FUNCTIONS : Sequence[BanRule] = (
    BanRule(
      'addTrackingRect:',
      (
       'The use of -[NSView addTrackingRect:owner:userData:assumeInside:] is'
       'prohibited. Please use CrTrackingArea instead.',
       'http://dev.chromium.org/developers/coding-style/cocoa-dos-and-donts',
      ),
      False,
    ),
    BanRule(
      r'/NSTrackingArea\W',
      (
       'The use of NSTrackingAreas is prohibited. Please use CrTrackingArea',
       'instead.',
       'http://dev.chromium.org/developers/coding-style/cocoa-dos-and-donts',
      ),
      False,
    ),
    BanRule(
      'convertPointFromBase:',
      (
       'The use of -[NSView convertPointFromBase:] is almost certainly wrong.',
       'Please use |convertPoint:(point) fromView:nil| instead.',
       'http://dev.chromium.org/developers/coding-style/cocoa-dos-and-donts',
      ),
      True,
    ),
    BanRule(
      'convertPointToBase:',
      (
       'The use of -[NSView convertPointToBase:] is almost certainly wrong.',
       'Please use |convertPoint:(point) toView:nil| instead.',
       'http://dev.chromium.org/developers/coding-style/cocoa-dos-and-donts',
      ),
      True,
    ),
    BanRule(
      'convertRectFromBase:',
      (
       'The use of -[NSView convertRectFromBase:] is almost certainly wrong.',
       'Please use |convertRect:(point) fromView:nil| instead.',
       'http://dev.chromium.org/developers/coding-style/cocoa-dos-and-donts',
      ),
      True,
    ),
    BanRule(
      'convertRectToBase:',
      (
       'The use of -[NSView convertRectToBase:] is almost certainly wrong.',
       'Please use |convertRect:(point) toView:nil| instead.',
       'http://dev.chromium.org/developers/coding-style/cocoa-dos-and-donts',
      ),
      True,
    ),
    BanRule(
      'convertSizeFromBase:',
      (
       'The use of -[NSView convertSizeFromBase:] is almost certainly wrong.',
       'Please use |convertSize:(point) fromView:nil| instead.',
       'http://dev.chromium.org/developers/coding-style/cocoa-dos-and-donts',
      ),
      True,
    ),
    BanRule(
      'convertSizeToBase:',
      (
       'The use of -[NSView convertSizeToBase:] is almost certainly wrong.',
       'Please use |convertSize:(point) toView:nil| instead.',
       'http://dev.chromium.org/developers/coding-style/cocoa-dos-and-donts',
      ),
      True,
    ),
    BanRule(
      r"/\s+UTF8String\s*]",
      (
       'The use of -[NSString UTF8String] is dangerous as it can return null',
       'even if |canBeConvertedToEncoding:NSUTF8StringEncoding| returns YES.',
       'Please use |SysNSStringToUTF8| instead.',
      ),
      True,
      excluded_paths = (
        '^third_party/ocmock/OCMock/',
      ),
    ),
    BanRule(
      r'__unsafe_unretained',
      (
        'The use of __unsafe_unretained is almost certainly wrong, unless',
        'when interacting with NSFastEnumeration or NSInvocation.',
        'Please use __weak in files build with ARC, nothing otherwise.',
      ),
      False,
    ),
    BanRule(
      'freeWhenDone:NO',
      (
        'The use of "freeWhenDone:NO" with the NoCopy creation of ',
        'Foundation types is prohibited.',
      ),
      True,
    ),
    BanRule(
      'This file requires ARC support.',
      (
        'ARC compilation is default in Chromium; do not add boilerplate to ',
        'files that require ARC.',
      ),
      True,
    ),
)

_BANNED_IOS_OBJC_FUNCTIONS = (
    BanRule(
      r'/\bTEST[(]',
      (
        'TEST() macro should not be used in Objective-C++ code as it does not ',
        'drain the autorelease pool at the end of the test. Use TEST_F() ',
        'macro instead with a fixture inheriting from PlatformTest (or a ',
        'typedef).'
      ),
      True,
    ),
    BanRule(
      r'/\btesting::Test\b',
      (
        'testing::Test should not be used in Objective-C++ code as it does ',
        'not drain the autorelease pool at the end of the test. Use ',
        'PlatformTest instead.'
      ),
      True,
    ),
    BanRule(
    ' systemImageNamed:',
      (
        '+[UIImage systemImageNamed:] should not be used to create symbols.',
        'Instead use a wrapper defined in:',
        'ios/chrome/browser/shared/ui/symbols/symbol_helpers.h'
      ),
      True,
      excluded_paths=(
        'ios/chrome/browser/shared/ui/symbols/symbol_helpers.mm',
        'ios/chrome/common',
        'ios/chrome/search_widget_extension/',
      ),
    ),
)

_BANNED_IOS_EGTEST_FUNCTIONS : Sequence[BanRule] = (
    BanRule(
      r'/\bEXPECT_OCMOCK_VERIFY\b',
      (
        'EXPECT_OCMOCK_VERIFY should not be used in EarlGrey tests because ',
        'it is meant for GTests. Use [mock verify] instead.'
      ),
      True,
    ),
)

_BANNED_CPP_FUNCTIONS: Sequence[BanRule] = (
    BanRule(
        '%#0',
        (
            'Zero-padded values that use "#" to add prefixes don\'t exhibit ',
            'consistent behavior, since the prefix is not prepended for zero ',
            'values. Use "0x%0..." instead.',
        ),
        False,
        [_THIRD_PARTY_EXCEPT_BLINK],  # Don't warn in third_party folders.
    ),
    BanRule(
        r'/\busing namespace ',
        (
            'Using directives ("using namespace x") are banned by the Google Style',
            'Guide ( http://google.github.io/styleguide/cppguide.html#Namespaces ).',
            'Explicitly qualify symbols or use using declarations ("using x::foo").',
        ),
        True,
        [_THIRD_PARTY_EXCEPT_BLINK],  # Don't warn in third_party folders.
    ),
    # Make sure that gtest's FRIEND_TEST() macro is not used; the
    # FRIEND_TEST_ALL_PREFIXES() macro from base/gtest_prod_util.h should be
    # used instead since that allows for FLAKY_ and DISABLED_ prefixes.
    BanRule(
        'FRIEND_TEST(',
        (
            'Chromium code should not use gtest\'s FRIEND_TEST() macro. Include',
            'base/gtest_prod_util.h and use FRIEND_TEST_ALL_PREFIXES() instead.',
        ),
        False,
        excluded_paths=(
            "base/gtest_prod_util.h",
            "base/allocator/partition_allocator/src/partition_alloc/partition_alloc_base/gtest_prod_util.h",
        ),
    ),
    BanRule(
        'setMatrixClip',
        (
            'Overriding setMatrixClip() is prohibited; ',
            'the base function is deprecated. ',
        ),
        True,
        (),
    ),
    BanRule(
        'SkRefPtr',
        ('The use of SkRefPtr is prohibited. ', 'Please use sk_sp<> instead.'),
        True,
        (),
    ),
    BanRule(
        'SkAutoRef',
        ('The indirect use of SkRefPtr via SkAutoRef is prohibited. ',
         'Please use sk_sp<> instead.'),
        True,
        (),
    ),
    BanRule(
        'SkAutoTUnref',
        ('The use of SkAutoTUnref is dangerous because it implicitly ',
         'converts to a raw pointer. Please use sk_sp<> instead.'),
        True,
        (),
    ),
    BanRule(
        'SkAutoUnref',
        ('The indirect use of SkAutoTUnref through SkAutoUnref is dangerous ',
         'because it implicitly converts to a raw pointer. ',
         'Please use sk_sp<> instead.'),
        True,
        (),
    ),
    BanRule(
        r'/HANDLE_EINTR\(.*close',
        ('HANDLE_EINTR(close) is invalid. If close fails with EINTR, the file',
         'descriptor will be closed, and it is incorrect to retry the close.',
         'Either call close directly and ignore its return value, or wrap close',
         'in IGNORE_EINTR to use its return value. See http://crbug.com/269623'
         ),
        True,
        (),
    ),
    BanRule(
        r'/IGNORE_EINTR\((?!.*close)',
        (
            'IGNORE_EINTR is only valid when wrapping close. To wrap other system',
            'calls, use HANDLE_EINTR. See http://crbug.com/269623',
        ),
        True,
        (
            # Files that #define IGNORE_EINTR.
            r'^base/posix/eintr_wrapper\.h$',
            r'^ppapi/tests/test_broker\.cc$',
        ),
    ),
    BanRule(
        r'/v8::Extension\(',
        (
            'Do not introduce new v8::Extensions into the code base, use',
            'gin::Wrappable instead. See http://crbug.com/334679',
        ),
        True,
        (r'extensions/renderer/safe_builtins\.*', ),
    ),
    BanRule(
        '#pragma comment(lib,',
        ('Specify libraries to link with in build files and not in the source.',
         ),
        True,
        (
            r'^base/third_party/symbolize/.*',
            r'^third_party/abseil-cpp/.*',
        ),
    ),
    BanRule(
        r'/base::SequenceChecker\b',
        ('Consider using SEQUENCE_CHECKER macros instead of the class directly.',
         ),
        False,
        (),
    ),
    BanRule(
        r'/base::ThreadChecker\b',
        ('Consider using THREAD_CHECKER macros instead of the class directly.',
         ),
        False,
        (),
    ),
    BanRule(
        r'/\b(?!(Sequenced|SingleThread))\w*TaskRunner::(GetCurrentDefault|CurrentDefaultHandle)',
        (
            'It is not allowed to call these methods from the subclasses ',
            'of Sequenced or SingleThread task runners.',
        ),
        True,
        (),
    ),
    BanRule(
        r'/(Time(|Delta|Ticks)|ThreadTicks)::FromInternalValue|ToInternalValue',
        (
            'base::TimeXXX::FromInternalValue() and ToInternalValue() are',
            'deprecated (http://crbug.com/634507). Please avoid converting away',
            'from the Time types in Chromium code, especially if any math is',
            'being done on time values. For interfacing with platform/library',
            'APIs, use base::Time::(From,To)DeltaSinceWindowsEpoch() or',
            'base::{TimeDelta::In}Microseconds(), or one of the other type',
            'converter methods instead. For faking TimeXXX values (for unit',
            'testing only), use TimeXXX() + Microseconds(N). For',
            'other use cases, please contact base/time/OWNERS.',
        ),
        False,
        excluded_paths=(
            "base/time/time.h",
            "base/allocator/partition_allocator/src/partition_alloc/partition_alloc_base/time/time.h",
        ),
    ),
    BanRule(
        'CallJavascriptFunctionUnsafe',
        (
            "Don't use CallJavascriptFunctionUnsafe() in new code. Instead, use",
            'AllowJavascript(), OnJavascriptAllowed()/OnJavascriptDisallowed(),',
            'and CallJavascriptFunction(). See https://goo.gl/qivavq.',
        ),
        False,
        (
            r'^content/browser/webui/web_ui_impl\.(cc|h)$',
            r'^content/public/browser/web_ui\.h$',
            r'^content/public/test/test_web_ui\.(cc|h)$',
        ),
    ),
    BanRule(
        'leveldb::DB::Open',
        (
            'Instead of leveldb::DB::Open() use leveldb_env::OpenDB() from',
            'third_party/leveldatabase/env_chromium.h. It exposes databases to',
            "Chrome's tracing, making their memory usage visible.",
        ),
        True,
        (r'^third_party/leveldatabase/.*\.(cc|h)$', ),
    ),
    BanRule(
        'leveldb::NewMemEnv',
        (
            'Instead of leveldb::NewMemEnv() use leveldb_chrome::NewMemEnv() from',
            'third_party/leveldatabase/leveldb_chrome.h. It exposes environments',
            "to Chrome's tracing, making their memory usage visible.",
        ),
        True,
        (r'^third_party/leveldatabase/.*\.(cc|h)$', ),
    ),
    BanRule(
        'RunLoop::QuitCurrent',
        (
            'Please migrate away from RunLoop::QuitCurrent*() methods. Use member',
            'methods of a specific RunLoop instance instead.',
        ),
        False,
        (),
    ),
    BanRule(
        'base::ScopedMockTimeMessageLoopTaskRunner',
        (
            'ScopedMockTimeMessageLoopTaskRunner is deprecated. Prefer',
            'TaskEnvironment::TimeSource::MOCK_TIME. There are still a',
            'few cases that may require a ScopedMockTimeMessageLoopTaskRunner',
            '(i.e. mocking the main MessageLoopForUI in browser_tests), but check',
            'with gab@ first if you think you need it)',
        ),
        False,
        (),
    ),
    BanRule(
        'std::regex',
        (
            'Using std::regex adds unnecessary binary size to Chrome. Please use',
            're2::RE2 instead (crbug.com/755321)',
        ),
        True,
        [
            # Abseil's benchmarks never linked into chrome.
            'third_party/abseil-cpp/.*_benchmark.cc',
        ],
    ),
    BanRule(
        r'/\bstd::sto(i|l|ul|ll|ull)\b',
        (
            'std::sto{i,l,ul,ll,ull}() use exceptions to communicate results. ',
            'Use base::StringTo[U]Int[64]() instead.',
        ),
        True,
        [_THIRD_PARTY_EXCEPT_BLINK],  # Don't warn in third_party folders.
    ),
    BanRule(
        r'/\bstd::sto(f|d|ld)\b',
        (
            'std::sto{f,d,ld}() use exceptions to communicate results. ',
            'For locale-independent values, e.g. reading numbers from disk',
            'profiles, use base::StringToDouble().',
            'For user-visible values, parse using ICU.',
        ),
        True,
        [_THIRD_PARTY_EXCEPT_BLINK],  # Don't warn in third_party folders.
    ),
    BanRule(
        r'/\bstd::to_string\b',
        (
            'std::to_string() is locale dependent and slower than alternatives.',
            'For locale-independent strings, e.g. writing numbers to disk',
            'profiles, use base::NumberToString().',
            'For user-visible strings, use base::FormatNumber() and',
            'the related functions in base/i18n/number_formatting.h.',
        ),
        True,
        [
            # TODO(crbug.com/335672557): Please do not add to this list. Existing
            # uses should removed.
            "base/linux_util.cc",
            "chrome/services/file_util/public/cpp/zip_file_creator_browsertest.cc",
            "chrome/test/chromedriver/chrome/web_view_impl.cc",
            "chrome/test/chromedriver/log_replay/log_replay_socket.cc",
            "chromecast/crash/linux/dump_info.cc",
            "chromeos/ash/components/dbus/biod/fake_biod_client.cc",
            "chromeos/ash/components/dbus/biod/fake_biod_client_unittest.cc",
            "chromeos/ash/components/report/utils/time_utils.cc",
            "chromeos/ash/services/device_sync/cryptauth_device_manager_impl.cc",
            "chromeos/ash/services/device_sync/cryptauth_device_manager_impl_unittest.cc",
            "chromeos/ash/services/secure_channel/ble_weave_packet_receiver.cc",
            "chromeos/ash/services/secure_channel/bluetooth_helper_impl_unittest.cc",
            "chromeos/process_proxy/process_proxy.cc",
            "components/chromeos_camera/jpeg_encode_accelerator_unittest.cc",
            "components/cronet/native/perftest/perf_test.cc",
            "components/download/internal/common/download_item_impl_unittest.cc",
            "components/gcm_driver/gcm_client_impl_unittest.cc",
            "components/history/core/test/fake_web_history_service.cc",
            "components/history_clusters/core/clustering_test_utils.cc",
            "components/language/content/browser/ulp_language_code_locator/s2langquadtree_datatest.cc",
            "components/live_caption/views/caption_bubble_controller_views.cc",
            "components/offline_pages/core/offline_event_logger_unittest.cc",
            "components/offline_pages/core/offline_page_model_event_logger.cc",
            "components/omnibox/browser/history_quick_provider_performance_unittest.cc",
            "components/omnibox/browser/in_memory_url_index_unittest.cc",
            "components/payments/content/payment_method_manifest_table_unittest.cc",
            "components/policy/core/common/cloud/device_management_service_unittest.cc",
            "components/policy/core/common/schema.cc",
            "components/sync_bookmarks/bookmark_model_observer_impl_unittest.cc",
            "components/tracing/test/trace_event_perftest.cc",
            "components/ui_devtools/views/overlay_agent_views.cc",
            "components/url_pattern_index/closed_hash_map_unittest.cc",
            "components/url_pattern_index/url_pattern_index_unittest.cc",
            "content/browser/accessibility/accessibility_tree_formatter_blink.cc",
            "content/browser/background_fetch/mock_background_fetch_delegate.cc",
            "content/browser/background_fetch/storage/database_helpers.cc",
            "content/browser/background_sync/background_sync_launcher_unittest.cc",
            "content/browser/browser_child_process_host_impl.cc",
            "content/browser/devtools/protocol/security_handler.cc",
            "content/browser/notifications/platform_notification_context_trigger_unittest.cc",
            "content/browser/renderer_host/input/touch_action_browsertest.cc",
            "content/browser/renderer_host/render_process_host_impl.cc",
            "content/browser/renderer_host/text_input_manager.cc",
            "content/browser/sandbox_parameters_mac.mm",
            "device/fido/mock_fido_device.cc",
            "gpu/command_buffer/tests/gl_webgl_multi_draw_test.cc",
            "gpu/config/gpu_control_list.cc",
            "media/audio/win/core_audio_util_win.cc",
            "media/gpu/android/media_codec_video_decoder.cc",
            "media/gpu/vaapi/vaapi_wrapper.cc",
            "remoting/host/linux/certificate_watcher_unittest.cc",
            "testing/libfuzzer/fuzzers/url_parse_proto_fuzzer.cc",
            "testing/libfuzzer/proto/url_proto_converter.cc",
            "third_party/blink/renderer/core/css/parser/css_proto_converter.cc",
            "third_party/blink/renderer/core/editing/ime/edit_context.cc",
            "third_party/blink/renderer/platform/graphics/bitmap_image_test.cc",
            "tools/binary_size/libsupersize/viewer/caspian/diff_test.cc",
            "tools/binary_size/libsupersize/viewer/caspian/tree_builder_test.cc",
            "ui/base/ime/win/tsf_text_store.cc",
            "ui/ozone/platform/drm/gpu/hardware_display_plane.cc",
            _THIRD_PARTY_EXCEPT_BLINK
        ],
    ),
    BanRule(
        r'/#include <(cctype|ctype\.h|cwctype|wctype.h)>',
        (
            '<cctype>/<ctype.h>/<cwctype>/<wctype.h> are banned. Use',
            '"third_party/abseil-cpp/absl/strings/ascii.h" instead.',
        ),
        True,
        [_THIRD_PARTY_EXCEPT_BLINK],  # Not an error in third_party folders.
    ),
    BanRule(
        r'/\bstd::shared_ptr\b',
        ('std::shared_ptr is banned. Use scoped_refptr instead.', ),
        True,
        [
            # Needed for interop with third-party library.
            '^third_party/blink/renderer/core/typed_arrays/array_buffer/' +
            'array_buffer_contents\.(cc|h)',
            '^third_party/blink/renderer/core/typed_arrays/dom_array_buffer\.cc',
            '^third_party/blink/renderer/bindings/core/v8/' +
            'v8_wasm_response_extensions.cc',
            '^gin/array_buffer\.(cc|h)',
            '^gin/per_isolate_data\.(cc|h)',
            '^chrome/services/sharing/nearby/',
            # Needed for interop with third-party library libunwindstack.
            '^base/profiler/libunwindstack_unwinder_android\.(cc|h)',
            '^base/profiler/native_unwinder_android_memory_regions_map_impl.(cc|h)',
            # Needed for interop with third-party boringssl cert verifier
            '^third_party/boringssl/',
            '^net/cert/',
            '^net/tools/cert_verify_tool/',
            '^services/cert_verifier/',
            '^components/certificate_transparency/',
            '^components/media_router/common/providers/cast/certificate/',
            # gRPC provides some C++ libraries that use std::shared_ptr<>.
            '^chromeos/ash/services/libassistant/grpc/',
            '^chromecast/cast_core/grpc',
            '^chromecast/cast_core/runtime/browser',
            '^ios/chrome/test/earl_grey/chrome_egtest_plugin_client\.(mm|h)',
            # Fuchsia provides C++ libraries that use std::shared_ptr<>.
            '^base/fuchsia/.*\.(cc|h)',
            '.*fuchsia.*test\.(cc|h)',
            # Clang plugins have different build config.
            '^tools/clang/plugins/',
            _THIRD_PARTY_EXCEPT_BLINK
        ],  # Not an error in third_party folders.
    ),
    BanRule(
        r'/\bstd::weak_ptr\b',
        ('std::weak_ptr is banned. Use base::WeakPtr instead.', ),
        True,
        [_THIRD_PARTY_EXCEPT_BLINK],  # Not an error in third_party folders.
    ),
    BanRule(
        r'/\blong long\b',
        ('long long is banned. Use [u]int64_t instead.', ),
        False,  # Only a warning since it is already used.
        [_THIRD_PARTY_EXCEPT_BLINK],  # Don't warn in third_party folders.
    ),
    BanRule(
        r'/\b(absl|std)::any\b',
        (
            '{absl,std}::any are banned due to incompatibility with the component ',
            'build.',
        ),
        True,
        # Not an error in third party folders, though it probably should be :)
        [_THIRD_PARTY_EXCEPT_BLINK],
    ),
    BanRule(
        r'/\bstd::bind\b',
        (
            'std::bind() is banned because of lifetime risks. Use ',
            'base::Bind{Once,Repeating}() instead.',
        ),
        True,
        [_THIRD_PARTY_EXCEPT_BLINK],  # Not an error in third_party folders.
    ),
    BanRule(
        (r'/\bstd::(?:'
         r'linear_congruential_engine|mersenne_twister_engine|'
         r'subtract_with_carry_engine|discard_block_engine|'
         r'independent_bits_engine|shuffle_order_engine|'
         r'minstd_rand0?|mt19937(_64)?|ranlux(24|48)(_base)?|knuth_b|'
         r'default_random_engine|'
         r'random_device|'
         r'seed_seq'
         r')\b'),
        (
            'STL random number engines and generators are banned. Use the ',
            'helpers in base/rand_util.h instead, e.g. base::RandBytes() or ',
            'base::RandomBitGenerator.'
            '',
            'Please reach out to cxx@chromium.org if the base APIs are ',
            'insufficient for your needs.',
        ),
        True,
        [
            # Not an error in third_party folders.
            _THIRD_PARTY_EXCEPT_BLINK,
            # Various tools which build outside of Chrome.
            r'testing/libfuzzer',
            r'testing/perf/confidence',
            r'tools/android/io_benchmark/',
            # Fuzzers are allowed to use standard library random number generators
            # since fuzzing speed + reproducibility is important.
            r'tools/ipc_fuzzer/',
            r'.+_fuzzer\.cc$',
            r'.+_fuzzertest\.cc$',
            # TODO(https://crbug.com/1380528): These are all unsanctioned uses of
            # the standard library's random number generators, and should be
            # migrated to the //base equivalent.
            r'ash/ambient/model/ambient_topic_queue\.cc',
            r'base/allocator/partition_allocator/src/partition_alloc/partition_alloc_unittest\.cc',
            r'base/ranges/algorithm_unittest\.cc',
            r'base/test/launcher/test_launcher\.cc',
            r'cc/metrics/video_playback_roughness_reporter_unittest\.cc',
            r'chrome/browser/apps/app_service/metrics/website_metrics\.cc',
            r'chrome/browser/ash/power/auto_screen_brightness/monotone_cubic_spline_unittest\.cc',
            r'chrome/browser/ash/printing/zeroconf_printer_detector_unittest\.cc',
            r'chrome/browser/nearby_sharing/contacts/nearby_share_contact_manager_impl_unittest\.cc',
            r'chrome/browser/nearby_sharing/contacts/nearby_share_contacts_sorter_unittest\.cc',
            r'chrome/browser/privacy_budget/mesa_distribution_unittest\.cc',
            r'chrome/browser/web_applications/test/web_app_test_utils\.cc',
            r'chrome/browser/web_applications/test/web_app_test_utils\.cc',
            r'chrome/browser/win/conflicts/module_blocklist_cache_util_unittest\.cc',
            r'chromeos/ash/components/memory/userspace_swap/swap_storage_unittest\.cc',
            r'chromeos/ash/components/memory/userspace_swap/userspace_swap\.cc',
            r'components/metrics/metrics_state_manager\.cc',
            r'components/omnibox/browser/history_quick_provider_performance_unittest\.cc',
            r'components/zucchini/disassembler_elf_unittest\.cc',
            r'content/browser/webid/federated_auth_request_impl\.cc',
            r'content/browser/webid/federated_auth_request_impl\.cc',
            r'media/cast/test/utility/udp_proxy\.h',
            r'sql/recover_module/module_unittest\.cc',
            r'components/search_engines/template_url_prepopulate_data.cc',
            # Do not add new entries to this list. If you have a use case which is
            # not satisfied by the current APIs (i.e. you need an explicitly-seeded
            # sequence, or stability of some sort is required), please contact
            # cxx@chromium.org.
        ],
    ),
    BanRule(
        r'/\b(absl,std)::bind_front\b',
        ('{absl,std}::bind_front() are banned. Use base::Bind{Once,Repeating}() '
         'instead.', ),
        True,
        [_THIRD_PARTY_EXCEPT_BLINK],  # Not an error in third_party folders.
    ),
    BanRule(
        r'/\bABSL_FLAG\b',
        ('ABSL_FLAG is banned. Use base::CommandLine instead.', ),
        True,
        [_THIRD_PARTY_EXCEPT_BLINK],  # Not an error in third_party folders.
    ),
    BanRule(
        r'/\babsl::c_',
        (
            'Abseil container utilities are banned. Use base/ranges/algorithm.h ',
            'instead.',
        ),
        True,
        [_THIRD_PARTY_EXCEPT_BLINK],  # Not an error in third_party folders.
    ),
    BanRule(
        r'/\babsl::FixedArray\b',
        ('absl::FixedArray is banned. Use base::FixedArray instead.', ),
        True,
        [
            # base::FixedArray provides canonical access.
            r'^base/types/fixed_array.h',
            # Not an error in third_party folders.
            _THIRD_PARTY_EXCEPT_BLINK,
        ],
    ),
    BanRule(
        r'/\babsl::FunctionRef\b',
        ('absl::FunctionRef is banned. Use base::FunctionRef instead.', ),
        True,
        [
            # base::Bind{Once,Repeating} references absl::FunctionRef to disallow
            # interoperability.
            r'^base/functional/bind_internal\.h',
            # base::FunctionRef is implemented on top of absl::FunctionRef.
            r'^base/functional/function_ref.*\..+',
            # Not an error in third_party folders.
            _THIRD_PARTY_EXCEPT_BLINK,
        ],
    ),
    BanRule(
        r'/\babsl::(Insecure)?BitGen\b',
        ('absl random number generators are banned. Use the helpers in '
         'base/rand_util.h instead, e.g. base::RandBytes() or ',
         'base::RandomBitGenerator.'),
        True,
        [_THIRD_PARTY_EXCEPT_BLINK],  # Not an error in third_party folders.
    ),
    BanRule(
        pattern=
        r'/\babsl::(optional|nullopt|make_optional)\b',
        explanation=('absl::optional is banned. Use std::optional instead.', ),
        treat_as_error=True,
        excluded_paths=[
            _THIRD_PARTY_EXCEPT_BLINK,
        ]),
    BanRule(
        r'/(\babsl::Span\b|#include <span>|\bstd::span\b)',
        (
            'absl::Span and std::span are not allowed ',
            '(https://crbug.com/1414652). Use base::span instead.',
        ),
        True,
        [
            # Included for conversions between base and std.
            r'base/containers/span.h',
            # Test base::span<> compatibility against std::span<>.
            r'base/containers/span_unittest.cc',
            # //base/numerics can't use base or absl. So it uses std.
            r'base/numerics/.*'

            # Needed to use QUICHE API.
            r'android_webview/browser/ip_protection/.*',
            r'chrome/browser/ip_protection/.*',
            r'components/ip_protection/.*',
            r'net/third_party/quiche/overrides/quiche_platform_impl/quiche_stack_trace_impl\.*',
            r'services/network/web_transport\.cc',

            # Not an error in third_party folders.
            _THIRD_PARTY_EXCEPT_BLINK,
        ],
    ),
    BanRule(
        r'/\babsl::StatusOr\b',
        ('absl::StatusOr is banned. Use base::expected instead.', ),
        True,
        [
            # Needed to use liburlpattern API.
            r'components/url_pattern/.*',
            r'services/network/shared_dictionary/simple_url_pattern_matcher\.cc',
            r'third_party/blink/renderer/core/url_pattern/.*',
            r'third_party/blink/renderer/modules/manifest/manifest_parser\.cc',

            # Needed to use QUICHE API.
            r'android_webview/browser/ip_protection/.*',
            r'chrome/browser/ip_protection/.*',
            r'components/ip_protection/.*',

            # Needed to use MediaPipe API.
            r'components/media_effects/.*\.cc',
            # Not an error in third_party folders.
            _THIRD_PARTY_EXCEPT_BLINK
        ],
    ),
    BanRule(
        r'/\babsl::(StrSplit|StrJoin|StrCat|StrAppend|Substitute|StrContains)\b',
        ('Abseil string utilities are banned. Use base/strings instead.', ),
        True,
        [_THIRD_PARTY_EXCEPT_BLINK],  # Not an error in third_party folders.
    ),
    BanRule(
        r'/\babsl::(Mutex|CondVar|Notification|Barrier|BlockingCounter)\b',
        (
            'Abseil synchronization primitives are banned. Use',
            'base/synchronization instead.',
        ),
        True,
        [_THIRD_PARTY_EXCEPT_BLINK],  # Not an error in third_party folders.
    ),
    BanRule(
        r'/\babsl::(Duration|Time|TimeZone|CivilDay)\b',
        ('Abseil\'s time library is banned. Use base/time instead.', ),
        True,
        [
            # Needed to use QUICHE API.
            r'android_webview/browser/ip_protection/.*',
            r'chrome/browser/ip_protection/.*',
            r'components/ip_protection/.*',

            # Needed to integrate with //third_party/nearby
            r'components/cross_device/nearby/system_clock.cc',
            _THIRD_PARTY_EXCEPT_BLINK  # Not an error in third_party folders.
        ],
    ),
    BanRule(
        r'/#include <chrono>',
        ('<chrono> is banned. Use base/time instead.', ),
        True,
        [
            # Not an error in third_party folders:
            _THIRD_PARTY_EXCEPT_BLINK,
            # This uses openscreen API depending on std::chrono.
            "components/openscreen_platform/task_runner.cc",
        ]),
    BanRule(
        r'/#include <exception>',
        ('Exceptions are banned and disabled in Chromium.', ),
        True,
        [_THIRD_PARTY_EXCEPT_BLINK],  # Not an error in third_party folders.
    ),
    BanRule(
        r'/\bstd::function\b',
        ('std::function is banned. Use base::{Once,Repeating}Callback instead.',
         ),
        True,
        [
            # Has tests that template trait helpers don't unintentionally match
            # std::function.
            r'base/functional/callback_helpers_unittest\.cc',
            # Required to implement interfaces from the third-party perfetto
            # library.
            r'base/tracing/perfetto_task_runner\.cc',
            r'base/tracing/perfetto_task_runner\.h',
            # Needed for interop with the third-party nearby library type
            # location::nearby::connections::ResultCallback.
            'chrome/services/sharing/nearby/nearby_connections_conversions\.cc'
            # Needed for interop with the internal libassistant library.
            'chromeos/ash/services/libassistant/callback_utils\.h',
            # Needed for interop with Fuchsia fidl APIs.
            'fuchsia_web/webengine/browser/context_impl_browsertest\.cc',
            'fuchsia_web/webengine/browser/cookie_manager_impl_unittest\.cc',
            'fuchsia_web/webengine/browser/media_player_impl_unittest\.cc',
            # Required to interop with interfaces from the third-party ChromeML
            # library API.
            'services/on_device_model/ml/chrome_ml_api\.h',
            'services/on_device_model/ml/on_device_model_executor\.cc',
            'services/on_device_model/ml/on_device_model_executor\.h',
            # Required to interop with interfaces from the third-party perfetto
            # library.
            'services/tracing/public/cpp/perfetto/custom_event_recorder\.cc',
            'services/tracing/public/cpp/perfetto/perfetto_traced_process\.cc',
            'services/tracing/public/cpp/perfetto/perfetto_traced_process\.h',
            'services/tracing/public/cpp/perfetto/perfetto_tracing_backend\.cc',
            'services/tracing/public/cpp/perfetto/producer_client\.cc',
            'services/tracing/public/cpp/perfetto/producer_client\.h',
            'services/tracing/public/cpp/perfetto/producer_test_utils\.cc',
            'services/tracing/public/cpp/perfetto/producer_test_utils\.h',
            # Required for interop with the third-party webrtc library.
            'third_party/blink/renderer/modules/peerconnection/mock_peer_connection_impl\.cc',
            'third_party/blink/renderer/modules/peerconnection/mock_peer_connection_impl\.h',
            # TODO(https://crbug.com/1364577): Various uses that should be
            # migrated to something else.
            # Should use base::OnceCallback or base::RepeatingCallback.
            'base/allocator/dispatcher/initializer_unittest\.cc',
            'chrome/browser/ash/accessibility/speech_monitor\.cc',
            'chrome/browser/ash/accessibility/speech_monitor\.h',
            'chrome/browser/ash/login/ash_hud_login_browsertest\.cc',
            'chromecast/base/observer_unittest\.cc',
            'chromecast/browser/cast_web_view\.h',
            'chromecast/public/cast_media_shlib\.h',
            'device/bluetooth/floss/exported_callback_manager\.h',
            'device/bluetooth/floss/floss_dbus_client\.h',
            'device/fido/cable/v2_handshake_unittest\.cc',
            'device/fido/pin\.cc',
            'services/tracing/perfetto/test_utils\.h',
            # Should use base::FunctionRef.
            'chrome/browser/media/webrtc/test_stats_dictionary\.cc',
            'chrome/browser/media/webrtc/test_stats_dictionary\.h',
            'chromeos/ash/services/libassistant/device_settings_controller\.cc',
            'components/browser_ui/client_certificate/android/ssl_client_certificate_request\.cc',
            'components/gwp_asan/client/sampling_malloc_shims_unittest\.cc',
            'content/browser/font_unique_name_lookup/font_unique_name_lookup_unittest\.cc',
            # Does not need std::function at all.
            'components/omnibox/browser/autocomplete_result\.cc',
            'device/fido/win/webauthn_api\.cc',
            'media/audio/alsa/alsa_util\.cc',
            'media/remoting/stream_provider\.h',
            'sql/vfs_wrapper\.cc',
            # TODO(https://crbug.com/1364585): Remove usage and exception list
            # entries.
            'extensions/renderer/api/automation/automation_internal_custom_bindings\.cc',
            'extensions/renderer/api/automation/automation_internal_custom_bindings\.h',
            # TODO(https://crbug.com/1364579): Remove usage and exception list
            # entry.
            'ui/views/controls/focus_ring\.h',

            # Various pre-existing uses in //tools that is low-priority to fix.
            'tools/binary_size/libsupersize/viewer/caspian/diff\.cc',
            'tools/binary_size/libsupersize/viewer/caspian/model\.cc',
            'tools/binary_size/libsupersize/viewer/caspian/model\.h',
            'tools/binary_size/libsupersize/viewer/caspian/tree_builder\.h',
            'tools/clang/base_bind_rewriters/BaseBindRewriters\.cpp',

            # Not an error in third_party folders.
            _THIRD_PARTY_EXCEPT_BLINK
        ],
    ),
    BanRule(
        r'/#include <X11/',
        ('Do not use Xlib. Use xproto (from //ui/gfx/x:xproto) instead.', ),
        True,
        [_THIRD_PARTY_EXCEPT_BLINK],  # Not an error in third_party folders.
    ),
    BanRule(
        r'/\bstd::ratio\b',
        ('std::ratio is banned by the Google Style Guide.', ),
        True,
        [_THIRD_PARTY_EXCEPT_BLINK],  # Not an error in third_party folders.
    ),
    BanRule(
        r'/\bstd::aligned_alloc\b',
        (
            'std::aligned_alloc() is not yet allowed (crbug.com/1412818). Use ',
            'base::AlignedAlloc() instead.',
        ),
        True,
        [_THIRD_PARTY_EXCEPT_BLINK],  # Don't warn in third_party folders.
    ),
    BanRule(
        r'/#include <(barrier|latch|semaphore|stop_token)>',
        ('The thread support library is banned. Use base/synchronization '
         'instead.', ),
        True,
        [_THIRD_PARTY_EXCEPT_BLINK],  # Don't warn in third_party folders.
    ),
    BanRule(
        r'/\bstd::execution::(par|seq)\b',
        ('std::execution::(par|seq) is banned; they do not fit into '
         ' Chrome\'s threading model, and libc++ doesn\'t have full '
         'support.', ),
        True,
        [_THIRD_PARTY_EXCEPT_BLINK],
    ),
    BanRule(
        r'/\bstd::bit_cast\b',
        ('std::bit_cast is banned; use base::bit_cast instead for values and '
         'standard C++ casting when pointers are involved.', ),
        True,
        [
            # Don't warn in third_party folders.
            _THIRD_PARTY_EXCEPT_BLINK,
            # //base/numerics can't use base or absl.
            r'base/numerics/.*'
        ],
    ),
    BanRule(
        r'/\bstd::(c8rtomb|mbrtoc8)\b',
        ('std::c8rtomb() and std::mbrtoc8() are banned.', ),
        True,
        [_THIRD_PARTY_EXCEPT_BLINK],  # Don't warn in third_party folders.
    ),
    BanRule(
        r'/\bchar8_t|std::u8string\b',
        (
            'char8_t and std::u8string are not yet allowed. Can you use [unsigned]',
            ' char and std::string instead?',
        ),
        True,
        [
            # The demangler does not use this type but needs to know about it.
            'base/third_party/symbolize/demangle\.cc',
            # Don't warn in third_party folders.
            _THIRD_PARTY_EXCEPT_BLINK
        ],
    ),
    BanRule(
        r'/(\b(co_await|co_return|co_yield)\b|#include <coroutine>)',
        ('Coroutines are not yet allowed (https://crbug.com/1403840).', ),
        True,
        [_THIRD_PARTY_EXCEPT_BLINK],  # Don't warn in third_party folders.
    ),
    BanRule(
        r'/^\s*(export\s|import\s+["<:\w]|module(;|\s+[:\w]))',
        ('Modules are disallowed for now due to lack of toolchain support.', ),
        True,
        [_THIRD_PARTY_EXCEPT_BLINK],  # Don't warn in third_party folders.
    ),
    BanRule(
        r'/\[\[(\w*::)?no_unique_address\]\]',
        (
            '[[no_unique_address]] does not work as expected on Windows ',
            '(https://crbug.com/1414621). Use NO_UNIQUE_ADDRESS instead.',
        ),
        True,
        [
            # NO_UNIQUE_ADDRESS / PA_NO_UNIQUE_ADDRESS provide canonical access.
            r'^base/compiler_specific\.h',
            r'^base/allocator/partition_allocator/src/partition_alloc/partition_alloc_base/compiler_specific\.h',
            # Not an error in third_party folders.
            _THIRD_PARTY_EXCEPT_BLINK,
        ],
    ),
    BanRule(
        r'/#include <format>',
        ('<format> is not yet allowed. Use base::StringPrintf() instead.', ),
        True,
        [_THIRD_PARTY_EXCEPT_BLINK],  # Don't warn in third_party folders.
    ),
    BanRule(
        pattern='std::views',
        explanation=(
            'Use of std::views is banned in Chrome. If you need this '
            'functionality, please contact cxx@chromium.org.',
        ),
        treat_as_error=True,
        excluded_paths=[
            # Don't warn in third_party folders.
            _THIRD_PARTY_EXCEPT_BLINK
        ],
    ),
    BanRule(
        # Ban everything except specifically allowlisted constructs.
        pattern=r'/std::ranges::(?!' + '|'.join((
            # From https://en.cppreference.com/w/cpp/ranges:
            # Range access
            'begin',
            'end',
            'cbegin',
            'cend',
            'rbegin',
            'rend',
            'crbegin',
            'crend',
            'size',
            'ssize',
            'empty',
            'data',
            'cdata',
            # Range primitives
            'iterator_t',
            'const_iterator_t',
            'sentinel_t',
            'const_sentinel_t',
            'range_difference_t',
            'range_size_t',
            'range_value_t',
            'range_reference_t',
            'range_const_reference_t',
            'range_rvalue_reference_t',
            'range_common_reference_t',
            # Dangling iterator handling
            'dangling',
            'borrowed_iterator_t',
            # Banned: borrowed_subrange_t
            # Range concepts
            'range',
            'borrowed_range',
            'sized_range',
            'view',
            'input_range',
            'output_range',
            'forward_range',
            'bidirectional_range',
            'random_access_range',
            'contiguous_range',
            'common_range',
            'viewable_range',
            'constant_range',
            # Banned: Views
            # Banned: Range factories
            # Banned: Range adaptors
            # From https://en.cppreference.com/w/cpp/algorithm/ranges:
            # Constrained algorithms: non-modifying sequence operations
            'all_of',
            'any_of',
            'none_of',
            'for_each',
            'for_each_n',
            'count',
            'count_if',
            'mismatch',
            'equal',
            'lexicographical_compare',
            'find',
            'find_if',
            'find_if_not',
            'find_end',
            'find_first_of',
            'adjacent_find',
            'search',
            'search_n',
            # Constrained algorithms: modifying sequence operations
            'copy',
            'copy_if',
            'copy_n',
            'copy_backward',
            'move',
            'move_backward',
            'fill',
            'fill_n',
            'transform',
            'generate',
            'generate_n',
            'remove',
            'remove_if',
            'remove_copy',
            'remove_copy_if',
            'replace',
            'replace_if',
            'replace_copy',
            'replace_copy_if',
            'swap_ranges',
            'reverse',
            'reverse_copy',
            'rotate',
            'rotate_copy',
            'shuffle',
            'sample',
            'unique',
            'unique_copy',
            # Constrained algorithms: partitioning operations
            'is_partitioned',
            'partition',
            'partition_copy',
            'stable_partition',
            'partition_point',
            # Constrained algorithms: sorting operations
            'is_sorted',
            'is_sorted_until',
            'sort',
            'partial_sort',
            'partial_sort_copy',
            'stable_sort',
            'nth_element',
            # Constrained algorithms: binary search operations (on sorted ranges)
            'lower_bound',
            'upper_bound',
            'binary_search',
            'equal_range',
            # Constrained algorithms: set operations (on sorted ranges)
            'merge',
            'inplace_merge',
            'includes',
            'set_difference',
            'set_intersection',
            'set_symmetric_difference',
            'set_union',
            # Constrained algorithms: heap operations
            'is_heap',
            'is_heap_until',
            'make_heap',
            'push_heap',
            'pop_heap',
            'sort_heap',
            # Constrained algorithms: minimum/maximum operations
            'max',
            'max_element',
            'min',
            'min_element',
            'minmax',
            'minmax_element',
            'clamp',
            # Constrained algorithms: permutation operations
            'is_permutation',
            'next_permutation',
            'prev_premutation',
            # Constrained uninitialized memory algorithms
            'uninitialized_copy',
            'uninitialized_copy_n',
            'uninitialized_fill',
            'uninitialized_fill_n',
            'uninitialized_move',
            'uninitialized_move_n',
            'uninitialized_default_construct',
            'uninitialized_default_construct_n',
            'uninitialized_value_construct',
            'uninitialized_value_construct_n',
            'destroy',
            'destroy_n',
            'destroy_at',
            'construct_at',
            # Return types
            'in_fun_result',
            'in_in_result',
            'in_out_result',
            'in_in_out_result',
            'in_out_out_result',
            'min_max_result',
            'in_found_result',
            # From https://en.cppreference.com/w/cpp/iterator
            'advance',
            'distance',
            'next',
            'prev',
        )) + r')\w+',
        explanation=(
            'Use of range views and associated helpers is banned in Chrome. '
            'If you need this functionality, please contact cxx@chromium.org.',
        ),
        treat_as_error=True,
        excluded_paths=[
            # Don't warn in third_party folders.
            _THIRD_PARTY_EXCEPT_BLINK
        ],
    ),
    BanRule(
        r'/#include <regex>',
        ('<regex> is not allowed. Use third_party/re2 instead.',
         ),
        True,
        [_THIRD_PARTY_EXCEPT_BLINK],  # Don't warn in third_party folders.
    ),
    BanRule(
        r'/#include <source_location>',
        ('<source_location> is not yet allowed. Use base/location.h instead.',
         ),
        True,
        [_THIRD_PARTY_EXCEPT_BLINK],  # Don't warn in third_party folders.
    ),
    BanRule(
        r'/\bstd::to_address\b',
        (
            'std::to_address is banned because it is not guaranteed to be',
            'SFINAE-compatible. Use base::to_address from base/types/to_address.h',
            'instead.',
        ),
        True,
        [
            # Needed in base::to_address implementation.
            r'base/types/to_address.h',
            _THIRD_PARTY_EXCEPT_BLINK
        ],  # Not an error in third_party folders.
    ),
    BanRule(
        r'/#include <syncstream>',
        ('<syncstream> is banned.', ),
        True,
        [_THIRD_PARTY_EXCEPT_BLINK],  # Don't warn in third_party folders.
    ),
    BanRule(
        r'/\bRunMessageLoop\b',
        ('RunMessageLoop is deprecated, use RunLoop instead.', ),
        False,
        (),
    ),
    BanRule(
        'RunAllPendingInMessageLoop()',
        (
            "Prefer RunLoop over RunAllPendingInMessageLoop, please contact gab@",
            "if you're convinced you need this.",
        ),
        False,
        (),
    ),
    BanRule(
        'RunAllPendingInMessageLoop(BrowserThread',
        (
            'RunAllPendingInMessageLoop is deprecated. Use RunLoop for',
            'BrowserThread::UI, BrowserTaskEnvironment::RunIOThreadUntilIdle',
            'for BrowserThread::IO, and prefer RunLoop::QuitClosure to observe',
            'async events instead of flushing threads.',
        ),
        False,
        (),
    ),
    BanRule(
        r'MessageLoopRunner',
        ('MessageLoopRunner is deprecated, use RunLoop instead.', ),
        False,
        (),
    ),
    BanRule(
        'GetDeferredQuitTaskForRunLoop',
        (
            "GetDeferredQuitTaskForRunLoop shouldn't be needed, please contact",
            "gab@ if you found a use case where this is the only solution.",
        ),
        False,
        (),
    ),
    BanRule(
        'sqlite3_initialize(',
        (
            'Instead of calling sqlite3_initialize(), depend on //sql, ',
            '#include "sql/initialize.h" and use sql::EnsureSqliteInitialized().',
        ),
        True,
        (
            r'^sql/initialization\.(cc|h)$',
            r'^third_party/sqlite/.*\.(c|cc|h)$',
        ),
    ),
    BanRule(
        'CREATE VIEW',
        (
            'SQL views are disabled in Chromium feature code',
            'https://chromium.googlesource.com/chromium/src/+/HEAD/sql#no-views',
        ),
        True,
        (
            _THIRD_PARTY_EXCEPT_BLINK,
            # sql/ itself uses views when using memory-mapped IO.
            r'^sql/.*',
            # Various performance tools that do not build as part of Chrome.
            r'^infra/.*',
            r'^tools/perf.*',
            r'.*perfetto.*',
        ),
    ),
    BanRule(
        'CREATE VIRTUAL TABLE',
        (
            'SQL virtual tables are disabled in Chromium feature code',
            'https://chromium.googlesource.com/chromium/src/+/HEAD/sql#no-virtual-tables',
        ),
        True,
        (
            _THIRD_PARTY_EXCEPT_BLINK,
            # sql/ itself uses virtual tables in the recovery module and tests.
            r'^sql/.*',
            # TODO(https://crbug.com/695592): Remove once WebSQL is deprecated.
            r'third_party/blink/web_tests/storage/websql/.*'
            # Various performance tools that do not build as part of Chrome.
            r'^tools/perf.*',
            r'.*perfetto.*',
        ),
    ),
    BanRule(
        'std::random_shuffle',
        ('std::random_shuffle is deprecated in C++14, and removed in C++17. Use',
         'base::RandomShuffle instead.'),
        True,
        (),
    ),
    BanRule(
        'ios/web/public/test/http_server',
        ('web::HTTPserver is deprecated use net::EmbeddedTestServer instead.',
         ),
        False,
        (),
    ),
    BanRule(
        'GetAddressOf',
        ('Improper use of Microsoft::WRL::ComPtr<T>::GetAddressOf() has been ',
         'implicated in a few leaks. ReleaseAndGetAddressOf() is safe but ',
         'operator& is generally recommended. So always use operator& instead. ',
         'See http://crbug.com/914910 for more conversion guidance.'),
        True,
        (),
    ),
    BanRule(
        'SHFileOperation',
        ('SHFileOperation was deprecated in Windows Vista, and there are less ',
         'complex functions to achieve the same goals. Use IFileOperation for ',
         'any esoteric actions instead.'),
        True,
        (),
    ),
    BanRule(
        'StringFromGUID2',
        ('StringFromGUID2 introduces an unnecessary dependency on ole32.dll.',
         'Use base::win::WStringFromGUID instead.'),
        True,
        (r'/base/win/win_util_unittest.cc', ),
    ),
    BanRule(
        'StringFromCLSID',
        ('StringFromCLSID introduces an unnecessary dependency on ole32.dll.',
         'Use base::win::WStringFromGUID instead.'),
        True,
        (r'/base/win/win_util_unittest.cc', ),
    ),
    BanRule(
        'kCFAllocatorNull',
        (
            'The use of kCFAllocatorNull with the NoCopy creation of ',
            'CoreFoundation types is prohibited.',
        ),
        True,
        (),
    ),
    BanRule(
        'mojo::ConvertTo',
        ('mojo::ConvertTo and TypeConverter are deprecated. Please consider',
         'StructTraits / UnionTraits / EnumTraits / ArrayTraits / MapTraits /',
         'StringTraits if you would like to convert between custom types and',
         'the wire format of mojom types.'),
        False,
        (
            r'^fuchsia_web/webengine/browser/url_request_rewrite_rules_manager\.cc$',
            r'^fuchsia_web/webengine/url_request_rewrite_type_converters\.cc$',
            r'^third_party/blink/.*\.(cc|h)$',
            r'^content/renderer/.*\.(cc|h)$',
        ),
    ),
    BanRule(
        'GetInterfaceProvider',
        ('InterfaceProvider is deprecated.',
         'Please use ExecutionContext::GetBrowserInterfaceBroker and overrides',
         'or Platform::GetBrowserInterfaceBroker.'),
        False,
        (),
    ),
    BanRule(
        'CComPtr',
        ('New code should use Microsoft::WRL::ComPtr from wrl/client.h as a ',
         'replacement for CComPtr from ATL. See http://crbug.com/5027 for more ',
         'details.'),
        False,
        (),
    ),
    BanRule(
        r'/\b(IFACE|STD)METHOD_?\(',
        ('IFACEMETHOD() and STDMETHOD() make code harder to format and read.',
         'Instead, always use IFACEMETHODIMP in the declaration.'),
        False,
        [_THIRD_PARTY_EXCEPT_BLINK],  # Not an error in third_party folders.
    ),
    BanRule(
        'set_owned_by_client',
        ('set_owned_by_client is deprecated.',
         'views::View already owns the child views by default. This introduces ',
         'a competing ownership model which makes the code difficult to reason ',
         'about. See http://crbug.com/1044687 for more details.'),
        False,
        (),
    ),
    BanRule(
        'RemoveAllChildViewsWithoutDeleting',
        ('RemoveAllChildViewsWithoutDeleting is deprecated.',
         'This method is deemed dangerous as, unless raw pointers are re-added,',
         'calls to this method introduce memory leaks.'),
        False,
        (),
    ),
    BanRule(
        r'/\bTRACE_EVENT_ASYNC_',
        (
            'Please use TRACE_EVENT_NESTABLE_ASYNC_.. macros instead',
            'of TRACE_EVENT_ASYNC_.. (crbug.com/1038710).',
        ),
        False,
        (
            r'^base/trace_event/.*',
            r'^base/tracing/.*',
        ),
    ),
    BanRule(
        r'/\bbase::debug::DumpWithoutCrashingUnthrottled[(][)]',
        (
            'base::debug::DumpWithoutCrashingUnthrottled() does not throttle',
            'dumps and may spam crash reports. Consider if the throttled',
            'variants suffice instead.',
        ),
        False,
        (),
    ),
    BanRule(
        'RoInitialize',
        ('Improper use of [base::win]::RoInitialize() has been implicated in a ',
         'few COM initialization leaks. Use base::win::ScopedWinrtInitializer ',
         'instead. See http://crbug.com/1197722 for more information.'),
        True,
        (
            r'^base/win/scoped_winrt_initializer\.cc$',
            r'^third_party/abseil-cpp/absl/.*',
        ),
    ),
    BanRule(
        r'base::Watchdog',
        (
            'base::Watchdog is deprecated because it creates its own thread.',
            'Instead, manually start a timer on a SequencedTaskRunner.',
        ),
        False,
        (),
    ),
    BanRule(
        'base::Passed',
        ('Do not use base::Passed. It is a legacy helper for capturing ',
         'move-only types with base::BindRepeating, but invoking the ',
         'resulting RepeatingCallback moves the captured value out of ',
         'the callback storage, and subsequent invocations may pass the ',
         'value in a valid but undefined state. Prefer base::BindOnce().',
         'See http://crbug.com/1326449 for context.'),
        False,
        (
            # False positive, but it is also fine to let bind internals reference
            # base::Passed.
            r'^base[\\/]functional[\\/]bind\.h',
            r'^base[\\/]functional[\\/]bind_internal\.h',
        ),
    ),
    BanRule(
        r'base::Feature k',
        ('Please use BASE_DECLARE_FEATURE() or BASE_FEATURE() instead of ',
         'directly declaring/defining features.'),
        True,
        [
            # Implements BASE_DECLARE_FEATURE().
            r'^base/feature_list\.h',
        ],
    ),
    BanRule(
        r'/\bchartorune\b',
        ('chartorune is not memory-safe, unless you can guarantee the input ',
         'string is always null-terminated. Otherwise, please use charntorune ',
         'from libphonenumber instead.'),
        True,
        [
            _THIRD_PARTY_EXCEPT_BLINK,
            # Exceptions to this rule should have a fuzzer.
        ],
    ),
    BanRule(
        r'/\b#include "base/atomicops\.h"\b',
        ('Do not use base::subtle atomics, but std::atomic, which are simpler '
         'to use, have better understood, clearer and richer semantics, and are '
         'harder to mis-use. See details in base/atomicops.h.', ),
        False,
        [_THIRD_PARTY_EXCEPT_BLINK],  # Not an error in third_party folders.
    ),
    BanRule(r'CrossThreadPersistent<', (
        'Do not use blink::CrossThreadPersistent, but '
        'blink::CrossThreadHandle. It is harder to mis-use.', 'More info: '
        'https://docs.google.com/document/d/1GIT0ysdQ84sGhIo1r9EscF_fFt93lmNVM_q4vvHj2FQ/edit#heading=h.3e4d6y61tgs',
        'Please contact platform-architecture-dev@ before adding new instances.'
    ), False, []),
    BanRule(r'CrossThreadWeakPersistent<', (
        'Do not use blink::CrossThreadWeakPersistent, but '
        'blink::CrossThreadWeakHandle. It is harder to mis-use.', 'More info: '
        'https://docs.google.com/document/d/1GIT0ysdQ84sGhIo1r9EscF_fFt93lmNVM_q4vvHj2FQ/edit#heading=h.3e4d6y61tgs',
        'Please contact platform-architecture-dev@ before adding new instances.'
    ), False, []),
    BanRule(r'objc/objc.h', (
        'Do not include <objc/objc.h>. It defines away ARC lifetime '
        'annotations, and is thus dangerous.',
        'Please use the pimpl pattern; search for `ObjCStorage` for examples.',
        'For further reading on how to safely mix C++ and Obj-C, see',
        'https://chromium.googlesource.com/chromium/src/+/main/docs/mac/mixing_cpp_and_objc.md'
    ), True, []),
    BanRule(
        r'/#include <filesystem>',
        ('libc++ <filesystem> is banned per the Google C++ styleguide.', ),
        True,
        # This fuzzing framework is a standalone open source project and
        # cannot rely on Chromium base.
        (r'third_party/centipede'),
    ),
    BanRule(
        r'TopDocument()',
        ('TopDocument() does not work correctly with out-of-process iframes. '
         'Please do not introduce new uses.', ),
        True,
        (
            # TODO(crbug.com/617677): Remove all remaining uses.
            r'^third_party/blink/renderer/core/dom/document\.cc',
            r'^third_party/blink/renderer/core/dom/document\.h',
            r'^third_party/blink/renderer/core/dom/element\.cc',
            r'^third_party/blink/renderer/core/exported/web_disallow_transition_scope_test\.cc',
            r'^third_party/blink/renderer/core/exported/web_document_test\.cc',
            r'^third_party/blink/renderer/core/html/html_anchor_element\.cc',
            r'^third_party/blink/renderer/core/html/html_dialog_element\.cc',
            r'^third_party/blink/renderer/core/html/html_element\.cc',
            r'^third_party/blink/renderer/core/html/html_frame_owner_element\.cc',
            r'^third_party/blink/renderer/core/html/media/video_wake_lock\.cc',
            r'^third_party/blink/renderer/core/loader/anchor_element_interaction_tracker\.cc',
            r'^third_party/blink/renderer/core/page/scrolling/root_scroller_controller\.cc',
            r'^third_party/blink/renderer/core/page/scrolling/top_document_root_scroller_controller\.cc',
            r'^third_party/blink/renderer/core/page/scrolling/top_document_root_scroller_controller\.h',
            r'^third_party/blink/renderer/core/script/classic_pending_script\.cc',
            r'^third_party/blink/renderer/core/script/script_loader\.cc',
        ),
    ),
    BanRule(
        pattern=r'base::raw_ptr<',
        explanation=('Do not use base::raw_ptr, use raw_ptr.', ),
        treat_as_error=True,
        excluded_paths=(
            '^base/',
            '^tools/',
        ),
    ),
    BanRule(
        pattern=r'base:raw_ref<',
        explanation=('Do not use base::raw_ref, use raw_ref.', ),
        treat_as_error=True,
        excluded_paths=(
            '^base/',
            '^tools/',
        ),
    ),
    BanRule(
        pattern=r'/raw_ptr<[^;}]*\w{};',
        explanation=(
            'Do not use {} for raw_ptr initialization, use = nullptr instead.',
        ),
        treat_as_error=True,
        excluded_paths=(
            '^base/',
            '^tools/',
        ),
    ),
    BanRule(
        pattern=r'/#include "base/allocator/.*/raw_'
        r'(ptr|ptr_cast|ptr_exclusion|ref).h"',
        explanation=(
            'Please include the corresponding facade headers:',
            '- #include "base/memory/raw_ptr.h"',
            '- #include "base/memory/raw_ptr_cast.h"',
            '- #include "base/memory/raw_ptr_exclusion.h"',
            '- #include "base/memory/raw_ref.h"',
        ),
        treat_as_error=True,
        excluded_paths=(
            '^base/',
            '^tools/',
        ),
    ),
    BanRule(
        pattern=r'ContentSettingsType::COOKIES',
        explanation=
        ('Do not use ContentSettingsType::COOKIES to check whether cookies are '
         'supported in the provided context. Instead rely on the '
         'content_settings::CookieSettings API. If you are using '
         'ContentSettingsType::COOKIES to check the user preference setting '
         'specifically, disregard this warning.', ),
        treat_as_error=False,
        excluded_paths=(
            '^chrome/browser/ui/content_settings/',
            '^components/content_settings/',
            '^services/network/cookie_settings.cc',
            '.*test.cc',
        ),
    ),
    BanRule(
        pattern=r'ContentSettingsType::TRACKING_PROTECTION',
        explanation=
        ('Do not directly use ContentSettingsType::TRACKING_PROTECTION to check '
         'for tracking protection exceptions. Instead rely on the '
         'privacy_sandbox::TrackingProtectionSettings API.', ),
        treat_as_error=False,
        excluded_paths=(
            '^chrome/browser/ui/content_settings/',
            '^components/content_settings/',
            '^components/privacy_sandbox/tracking_protection_settings.cc',
            '.*test.cc',
        ),
    ),
    BanRule(
        pattern=r'/\bg_signal_connect',
        explanation=('Use ScopedGSignal instead of g_signal_connect*()', ),
        treat_as_error=True,
        excluded_paths=('^ui/base/glib/scoped_gsignal.h', ),
    ),
    BanRule(
        pattern=r'features::kIsolatedWebApps',
        explanation=(
            'Do not use `features::kIsolatedWebApps` directly to guard Isolated ',
            'Web App code. ',
            'Use `content::IsolatedWebAppsPolicy::AreIsolatedWebAppsEnabled()` in ',
            'the browser process or check the `kEnableIsolatedWebAppsInRenderer` ',
            'command line flag in the renderer process.',
        ),
        treat_as_error=True,
        excluded_paths=_TEST_CODE_EXCLUDED_PATHS +
        ('^chrome/browser/about_flags.cc',
         '^chrome/browser/web_applications/isolated_web_apps/chrome_content_browser_client_isolated_web_apps_part.cc',
         '^chrome/browser/ui/startup/bad_flags_prompt.cc',
         '^content/shell/browser/shell_content_browser_client.cc')),
    BanRule(
        pattern=r'features::kIsolatedWebAppDevMode',
        explanation=(
            'Do not use `features::kIsolatedWebAppDevMode` directly to guard code ',
            'related to Isolated Web App Developer Mode. ',
            'Use `web_app::IsIwaDevModeEnabled()` instead.',
        ),
        treat_as_error=True,
        excluded_paths=_TEST_CODE_EXCLUDED_PATHS + (
            '^chrome/browser/about_flags.cc',
            '^chrome/browser/web_applications/isolated_web_apps/isolated_web_app_features.cc',
            '^chrome/browser/ui/startup/bad_flags_prompt.cc',
        )),
    BanRule(
        pattern=r'features::kIsolatedWebAppUnmanagedInstall',
        explanation=(
            'Do not use `features::kIsolatedWebAppUnmanagedInstall` directly to ',
            'guard code related to unmanaged install flow for Isolated Web Apps. ',
            'Use `web_app::IsIwaUnmanagedInstallEnabled()` instead.',
        ),
        treat_as_error=True,
        excluded_paths=_TEST_CODE_EXCLUDED_PATHS + (
            '^chrome/browser/about_flags.cc',
            '^chrome/browser/web_applications/isolated_web_apps/isolated_web_app_features.cc',
        )),
    BanRule(
        pattern='/(CUIAutomation|AccessibleObjectFromWindow)',
        explanation=
        ('Direct usage of UIAutomation or IAccessible2 in client code is '
         'discouraged in Chromium, as it is not an assistive technology and '
         'should not rely on accessibility APIs directly. These APIs can '
         'introduce significant performance overhead. However, if you believe '
         'your use case warrants an exception, please discuss it with an '
         'accessibility owner before proceeding. For more information on the '
         'performance implications, see https://docs.google.com/document/d/1jN4itpCe_bDXF0BhFaYwv4xVLsCWkL9eULdzjmLzkuk/edit#heading=h.pwth3nbwdub0.',
         ),
        treat_as_error=False,
    ),
    BanRule(
        pattern=r'/WIDGET_OWNS_NATIVE_WIDGET|'
        r'NATIVE_WIDGET_OWNS_WIDGET',
        explanation=
        ('WIDGET_OWNS_NATIVE_WIDGET and NATIVE_WIDGET_OWNS_WIDGET are in the '
         'process of being deprecated. Consider using the new '
         'CLIENT_OWNS_WIDGET ownership model. Eventually, this will be the only '
         'available ownership model available and the associated enumeration'
         'will be removed.', ),
        treat_as_error=False,
    ),
    BanRule(
        pattern='ProfileManager::GetLastUsedProfile',
        explanation=
        ('Most code should already be scoped to a Profile. Pass in a Profile* '
         'or retreive from an existing entity with a reference to the Profile '
         '(e.g. WebContents).', ),
        treat_as_error=False,
    ),
    BanRule(
        pattern=(r'/FindBrowserWithUiElementContext|'
                 r'FindBrowserWithTab|'
                 r'FindBrowserWithGroup|'
                 r'FindTabbedBrowser|'
                 r'FindAnyBrowser|'
                 r'FindBrowserWithProfile|'
                 r'FindLastActive|'
                 r'FindBrowserWithActiveWindow'),
        explanation=
        ('Most code should already be scoped to a Browser. Pass in a Browser* '
         'or retreive from an existing entity with a reference to the Browser.',
         ),
        treat_as_error=False,
    ),
    BanRule(
        pattern='BrowserUserData',
        explanation=
        ('Do not use BrowserUserData to store state on a Browser instance. '
         'Instead use BrowserWindowFeatures. BrowserWindowFeatures is '
         'functionally identical but has two benefits: it does not force a '
         'dependency onto class Browser, and lifetime semantics are explicit '
         'rather than implicit. See BrowserUserData header file for more '
         'details.', ),
        treat_as_error=False,
        excluded_paths=(
          # Exclude iOS as the iOS implementation of BrowserUserData is separate
          # and still in use.
          '^ios/',
        ),
    ),
    BanRule(
        pattern=r'UNSAFE_TODO(',
        explanation=
        ('Do not use UNSAFE_TODO() to write new unsafe code. Use only when '
         'removing a pre-existing file-wide allow_unsafe_buffers pragma, or '
         'when incrementally converting code off of unsafe interfaces',
        ),
        treat_as_error=False,
    ),
    BanRule(
        pattern=r'UNSAFE_BUFFERS(',
        explanation=
        ('Try to avoid using UNSAFE_BUFFERS() if at all possible. Otherwise, '
         'be sure to justify in a // SAFETY comment why other options are not '
         'available, and why the code is safe.',
        ),
        treat_as_error=False,
    ),
    BanRule(
        pattern='BrowserWithTestWindowTest',
        explanation=
        ('Do not use BrowserWithTestWindowTest. By instantiating an instance '
         'of class Browser, the test is no longer a unit test but is instead a '
         'browser test. The class BrowserWithTestWindowTest forces production '
         'logic to take on test-only conditionals, which is an anti-pattern. '
         'Features should be performing dependency injection rather than '
         'directly using class Browser. See '
         'docs/chrome_browser_design_principles.md for more details.',
        ),
        treat_as_error=False,
    ),
    BanRule(
        pattern='TestWithBrowserView',
        explanation=
         ('Do not use TestWithBrowserView. See '
          'docs/chrome_browser_design_principles.md for details. If you want '
          'to write a test that has both a Browser and a BrowserView, create '
          'a browser_test. If you want to write a unit_test, your code must '
          'not reference Browser*.',
         ),
        treat_as_error=False,
    ),
    BanRule(
        pattern='RunUntilIdle',
        explanation=
        ('Do not RunUntilIdle. If possible, explicitly quit the run loop using '
         'run_loop.Quit() or run_loop.QuitClosure() if completion can be '
         'observed using a lambda or callback. Otherwise, wait for the '
         'condition to be true via base::test::RunUntil().',
        ),
        treat_as_error=False,
    ),
    BanRule(
        pattern=r'/\bstd::(literals|string_literals|string_view_literals)\b',
        explanation = (
            'User-defined literals are banned by the Google C++ style guide. '
            'Exceptions are provided in Chrome for string and string_view '
            'literals that embed \\0.',
        ),
        treat_as_error=True,
        excluded_paths=(
            # Various tests or test helpers that embed NUL in strings or
            # string_views.
            r'^ash/components/arc/session/serial_number_util_unittest\.cc',
            r'^base/strings/string_util_unittest\.cc',
            r'^base/strings/utf_string_conversions_unittest\.cc',
            r'^chrome/browser/ash/crosapi/browser_data_back_migrator_unittest\.cc',
            r'^chrome/browser/ash/crosapi/browser_data_migrator_util_unittest\.cc',
            r'^chrome/browser/ash/crosapi/move_migrator_unittest\.cc',
            r'^components/history/core/browser/visit_annotations_database\.cc',
            r'^components/history/core/browser/visit_annotations_database_unittest\.cc',
            r'^components/os_crypt/sync/os_crypt_unittest\.cc',
            r'^components/password_manager/core/browser/credentials_cleaner_unittest\.cc',
            r'^content/browser/file_system_access/file_system_access_file_writer_impl_unittest\.cc',
            r'^net/cookies/parsed_cookie_unittest\.cc',
            r'^third_party/blink/renderer/modules/webcodecs/test_helpers\.cc',
            r'^third_party/blink/renderer/modules/websockets/websocket_channel_impl_test\.cc',
        ),
    ),
    BanRule(
        pattern='BUILDFLAG(GOOGLE_CHROME_BRANDING)',
        explanation=
        ('Code gated by GOOGLE_CHROME_BRANDING is effectively untested. This '
         'is typically wrong. Valid use cases are glue for private modules '
         'shipped alongside Chrome, and installation-related logic.',
        ),
        treat_as_error=False,
    ),
    BanRule(
        pattern='defined(OFFICIAL_BUILD)',
        explanation=
        ('Code gated by OFFICIAL_BUILD is effectively untested. This '
         'is typically wrong. One valid use case is low-level code that '
         'handles subtleties related to high-levels of optimizations that come '
         'with OFFICIAL_BUILD.',
        ),
        treat_as_error=False,
    ),
)

_DEPRECATED_SYNC_CONSENT_FUNCTION_WARNING = (
  'Used a predicate related to signin::ConsentLevel::kSync which will always '
  'return false in the future (crbug.com/40066949). Prefer using a predicate '
  'that also supports signin::ConsentLevel::kSignin when appropriate. It is '
  'safe to ignore this warning if you are just moving an existing call, or if '
  'you want special handling for users in the legacy state. In doubt, reach '
  'out to //components/sync/OWNERS.',
)

# C++ functions related to signin::ConsentLevel::kSync which are deprecated.
_DEPRECATED_SYNC_CONSENT_CPP_FUNCTIONS : Sequence[BanRule] = (
    BanRule(
      'HasSyncConsent',
      _DEPRECATED_SYNC_CONSENT_FUNCTION_WARNING,
      False,
    ),
    BanRule(
      'CanSyncFeatureStart',
      _DEPRECATED_SYNC_CONSENT_FUNCTION_WARNING,
      False,
    ),
    BanRule(
      'IsSyncFeatureEnabled',
      _DEPRECATED_SYNC_CONSENT_FUNCTION_WARNING,
      False,
    ),
    BanRule(
      'IsSyncFeatureActive',
      _DEPRECATED_SYNC_CONSENT_FUNCTION_WARNING,
      False,
    ),
)

# Java functions related to signin::ConsentLevel::kSync which are deprecated.
_DEPRECATED_SYNC_CONSENT_JAVA_FUNCTIONS : Sequence[BanRule] = (
    BanRule(
      'hasSyncConsent',
      _DEPRECATED_SYNC_CONSENT_FUNCTION_WARNING,
      False,
    ),
    BanRule(
      'canSyncFeatureStart',
      _DEPRECATED_SYNC_CONSENT_FUNCTION_WARNING,
      False,
    ),
    BanRule(
      'isSyncFeatureEnabled',
      _DEPRECATED_SYNC_CONSENT_FUNCTION_WARNING,
      False,
    ),
    BanRule(
      'isSyncFeatureActive',
      _DEPRECATED_SYNC_CONSENT_FUNCTION_WARNING,
      False,
    ),
)

_BANNED_MOJOM_PATTERNS : Sequence[BanRule] = (
    BanRule(
        'handle<shared_buffer>',
        (
         'Please use one of the more specific shared memory types instead:',
         '  mojo_base.mojom.ReadOnlySharedMemoryRegion',
         '  mojo_base.mojom.WritableSharedMemoryRegion',
         '  mojo_base.mojom.UnsafeSharedMemoryRegion',
        ),
        True,
    ),
)

_IPC_ENUM_TRAITS_DEPRECATED = (
    'You are using IPC_ENUM_TRAITS() in your code. It has been deprecated.\n'
    'See http://www.chromium.org/Home/chromium-security/education/'
    'security-tips-for-ipc')

_LONG_PATH_ERROR = (
    'Some files included in this CL have file names that are too long (> 200'
    ' characters). If committed, these files will cause issues on Windows. See'
    ' https://crbug.com/612667 for more details.'
)

_JAVA_MULTIPLE_DEFINITION_EXCLUDED_PATHS = [
    r".*/BuildHooksAndroidImpl\.java",
    r".*/LicenseContentProvider\.java",
    r".*/PlatformServiceBridgeImpl.java",
    r".*chrome/android/feed/dummy/.*\.java",
]

# List of image extensions that are used as resources in chromium.
_IMAGE_EXTENSIONS = ['.svg', '.png', '.webp']

# These paths contain test data and other known invalid JSON files.
_KNOWN_TEST_DATA_AND_INVALID_JSON_FILE_PATTERNS = [
    r'test/data/',
    r'testing/buildbot/',
    r'^components/policy/resources/policy_templates\.json$',
    r'^third_party/protobuf/',
    r'^third_party/blink/perf_tests/speedometer.*/resources/todomvc/learn\.json',
    r'^third_party/blink/renderer/devtools/protocol\.json$',
    r'^third_party/blink/web_tests/external/wpt/',
    r'^tools/perf/',
    r'^tools/traceline/svgui/startup-release.json',
    # vscode configuration files allow comments
    r'^tools/vscode/',
]

# These are not checked on the public chromium-presubmit trybot.
# Add files here that rely on .py files that exists only for target_os="android"
# checkouts.
_ANDROID_SPECIFIC_PYDEPS_FILES = [
    'chrome/android/features/create_stripped_java_factory.pydeps',
]


_GENERIC_PYDEPS_FILES = [
    'android_webview/tools/run_cts.pydeps',
    'build/android/apk_operations.pydeps',
    'build/android/devil_chromium.pydeps',
    'build/android/gyp/aar.pydeps',
    'build/android/gyp/aidl.pydeps',
    'build/android/gyp/apkbuilder.pydeps',
    'build/android/gyp/assert_static_initializers.pydeps',
    'build/android/gyp/binary_baseline_profile.pydeps',
    'build/android/gyp/bytecode_processor.pydeps',
    'build/android/gyp/bytecode_rewriter.pydeps',
    'build/android/gyp/check_flag_expectations.pydeps',
    'build/android/gyp/compile_java.pydeps',
    'build/android/gyp/compile_kt.pydeps',
    'build/android/gyp/compile_resources.pydeps',
    'build/android/gyp/copy_ex.pydeps',
    'build/android/gyp/create_apk_operations_script.pydeps',
    'build/android/gyp/create_app_bundle.pydeps',
    'build/android/gyp/create_app_bundle_apks.pydeps',
    'build/android/gyp/create_bundle_wrapper_script.pydeps',
    'build/android/gyp/create_java_binary_script.pydeps',
    'build/android/gyp/create_r_java.pydeps',
    'build/android/gyp/create_r_txt.pydeps',
    'build/android/gyp/create_size_info_files.pydeps',
    'build/android/gyp/create_test_apk_wrapper_script.pydeps',
    'build/android/gyp/create_ui_locale_resources.pydeps',
    'build/android/gyp/dex.pydeps',
    'build/android/gyp/dist_aar.pydeps',
    'build/android/gyp/filter_zip.pydeps',
    'build/android/gyp/flatc_java.pydeps',
    'build/android/gyp/gcc_preprocess.pydeps',
    'build/android/gyp/generate_linker_version_script.pydeps',
    'build/android/gyp/ijar.pydeps',
    'build/android/gyp/jacoco_instr.pydeps',
    'build/android/gyp/java_cpp_enum.pydeps',
    'build/android/gyp/java_cpp_features.pydeps',
    'build/android/gyp/java_cpp_strings.pydeps',
    'build/android/gyp/java_google_api_keys.pydeps',
    'build/android/gyp/jinja_template.pydeps',
    'build/android/gyp/lint.pydeps',
    'build/android/gyp/merge_manifest.pydeps',
    'build/android/gyp/optimize_resources.pydeps',
    'build/android/gyp/prepare_resources.pydeps',
    'build/android/gyp/process_native_prebuilt.pydeps',
    'build/android/gyp/proguard.pydeps',
    'build/android/gyp/rename_java_classes.pydeps',
    'build/android/gyp/system_image_apks.pydeps',
    'build/android/gyp/trace_event_bytecode_rewriter.pydeps',
    'build/android/gyp/turbine.pydeps',
    'build/android/gyp/unused_resources.pydeps',
    'build/android/gyp/validate_static_library_dex_references.pydeps',
    'build/android/gyp/write_build_config.pydeps',
    'build/android/gyp/write_native_libraries_java.pydeps',
    'build/android/gyp/zip.pydeps',
    'build/android/incremental_install/generate_android_manifest.pydeps',
    'build/android/incremental_install/write_installer_json.pydeps',
    'build/android/pylib/results/presentation/test_results_presentation.pydeps',
    'build/android/resource_sizes.pydeps',
    'build/android/test_runner.pydeps',
    'build/android/test_wrapper/logdog_wrapper.pydeps',
    'build/lacros/lacros_resource_sizes.pydeps',
    'build/protoc_java.pydeps',
    'chrome/android/monochrome/scripts/monochrome_python_tests.pydeps',
    'chrome/test/chromedriver/log_replay/client_replay_unittest.pydeps',
    'chrome/test/chromedriver/test/run_py_tests.pydeps',
    'chromecast/resource_sizes/chromecast_resource_sizes.pydeps',
    'components/cronet/tools/check_combined_proguard_file.pydeps',
    'components/cronet/tools/generate_proguard_file.pydeps',
    'components/cronet/tools/jar_src.pydeps',
    'components/module_installer/android/module_desc_java.pydeps',
    'content/public/android/generate_child_service.pydeps',
    'fuchsia_web/av_testing/av_sync_tests.pydeps',
    'net/tools/testserver/testserver.pydeps',
    'testing/scripts/run_isolated_script_test.pydeps',
    'testing/merge_scripts/standard_isolated_script_merge.pydeps',
    'testing/merge_scripts/standard_gtest_merge.pydeps',
    'testing/merge_scripts/code_coverage/merge_results.pydeps',
    'testing/merge_scripts/code_coverage/merge_steps.pydeps',
    'third_party/android_platform/development/scripts/stack.pydeps',
    'third_party/blink/renderer/bindings/scripts/build_web_idl_database.pydeps',
    'third_party/blink/renderer/bindings/scripts/check_generated_file_list.pydeps',
    'third_party/blink/renderer/bindings/scripts/collect_idl_files.pydeps',
    'third_party/blink/renderer/bindings/scripts/generate_bindings.pydeps',
    'third_party/blink/renderer/bindings/scripts/generate_event_interface_names.pydeps',
    'third_party/blink/renderer/bindings/scripts/validate_web_idl.pydeps',
    'third_party/blink/tools/blinkpy/web_tests/merge_results.pydeps',
    'third_party/blink/tools/merge_web_test_results.pydeps',
    'tools/binary_size/sizes.pydeps',
    'tools/binary_size/supersize.pydeps',
    'tools/perf/process_perf_results.pydeps',
    'tools/pgo/generate_profile.pydeps',
]


_ALL_PYDEPS_FILES = _ANDROID_SPECIFIC_PYDEPS_FILES + _GENERIC_PYDEPS_FILES


# Bypass the AUTHORS check for these accounts.
_KNOWN_ROBOTS = set(
  ) | set('%s@appspot.gserviceaccount.com' % s for s in ('findit-for-me', 'luci-bisection')
  ) | set('%s@developer.gserviceaccount.com' % s for s in ('3su6n15k.default',)
  ) | set('%s@chops-service-accounts.iam.gserviceaccount.com' % s
          for s in ('bling-autoroll-builder', 'v8-ci-autoroll-builder',
                    'wpt-autoroller', 'chrome-weblayer-builder',
                    'lacros-version-skew-roller', 'skylab-test-cros-roller',
                    'infra-try-recipes-tester', 'lacros-tracking-roller',
                    'lacros-sdk-version-roller', 'chrome-automated-expectation',
                    'chromium-automated-expectation', 'chrome-branch-day',
                    'chromium-autosharder')
  ) | set('%s@skia-public.iam.gserviceaccount.com' % s
          for s in ('chromium-autoroll', 'chromium-release-autoroll')
  ) | set('%s@skia-corp.google.com.iam.gserviceaccount.com' % s
          for s in ('chromium-internal-autoroll',)
  ) | set('%s@system.gserviceaccount.com' % s
          for s in ('chrome-screen-ai-releaser',)
  ) | set('%s@owners-cleanup-prod.google.com.iam.gserviceaccount.com' % s
          for s in ('swarming-tasks',)
  ) | set('%s@fuchsia-infra.iam.gserviceaccount.com' % s
          for s in ('global-integration-try-builder',
                    'global-integration-ci-builder')
  ) | set('%s@prod.google.com' % s
          for s in ('chops-security-borg',
                    'chops-security-cronjobs-cpesuggest'))

_INVALID_GRD_FILE_LINE = [
        (r'<file lang=.* path=.*', 'Path should come before lang in GRD files.')
]

def _IsCPlusPlusFile(input_api, file_path):
    """Returns True if this file contains C++-like code (and not Python,
    Go, Java, MarkDown, ...)"""

    ext = input_api.os_path.splitext(file_path)[1]
    # This list is compatible with CppChecker.IsCppFile but we should
    # consider adding ".c" to it. If we do that we can use this function
    # at more places in the code.
    return ext in (
        '.h',
        '.cc',
        '.cpp',
        '.m',
        '.mm',
    )


def _IsCPlusPlusHeaderFile(input_api, file_path):
    return input_api.os_path.splitext(file_path)[1] == ".h"


def _IsJavaFile(input_api, file_path):
    return input_api.os_path.splitext(file_path)[1] == ".java"


def _IsProtoFile(input_api, file_path):
    return input_api.os_path.splitext(file_path)[1] == ".proto"


def _IsXmlOrGrdFile(input_api, file_path):
    ext = input_api.os_path.splitext(file_path)[1]
    return ext in ('.grd', '.xml')


def _IsMojomFile(input_api, file_path):
    return input_api.os_path.splitext(file_path)[1] == ".mojom"


def CheckNoUpstreamDepsOnClank(input_api, output_api):
    """Prevent additions of dependencies from the upstream repo on //clank."""
    # clank can depend on clank
    if input_api.change.RepositoryRoot().endswith('clank'):
        return []
    build_file_patterns = [
        r'(.+/)?BUILD\.gn',
        r'.+\.gni',
    ]
    excluded_files = [r'build[/\\]config[/\\]android[/\\]config\.gni']
    bad_pattern = input_api.re.compile(r'^[^#]*//clank')

    error_message = 'Disallowed import on //clank in an upstream build file:'

    def FilterFile(affected_file):
        return input_api.FilterSourceFile(affected_file,
                                          files_to_check=build_file_patterns,
                                          files_to_skip=excluded_files)

    problems = []
    for f in input_api.AffectedSourceFiles(FilterFile):
        local_path = f.LocalPath()
        for line_number, line in f.ChangedContents():
            if (bad_pattern.search(line)):
                problems.append('%s:%d\n    %s' %
                                (local_path, line_number, line.strip()))
    if problems:
        return [output_api.PresubmitPromptOrNotify(error_message, problems)]
    else:
        return []


def CheckNoProductionCodeUsingTestOnlyFunctions(input_api, output_api):
    """Attempts to prevent use of functions intended only for testing in
    non-testing code. For now this is just a best-effort implementation
    that ignores header files and may have some false positives. A
    better implementation would probably need a proper C++ parser.
    """
    # We only scan .cc files and the like, as the declaration of
    # for-testing functions in header files are hard to distinguish from
    # calls to such functions without a proper C++ parser.
    file_inclusion_pattern = [r'.+%s' % _IMPLEMENTATION_EXTENSIONS]

    base_function_pattern = r'[ :]test::[^\s]+|ForTest(s|ing)?|for_test(s|ing)?'
    inclusion_pattern = input_api.re.compile(r'(%s)\s*\(' %
                                             base_function_pattern)
    comment_pattern = input_api.re.compile(r'//.*(%s)' % base_function_pattern)
    allowlist_pattern = input_api.re.compile(r'// IN-TEST$')
    exclusion_pattern = input_api.re.compile(
        r'::[A-Za-z0-9_]+(%s)|(%s)[^;]+\{' %
        (base_function_pattern, base_function_pattern))
    # Avoid a false positive in this case, where the method name, the ::, and
    # the closing { are all on different lines due to line wrapping.
    # HelperClassForTesting::
    #   HelperClassForTesting(
    #       args)
    #     : member(0) {}
    method_defn_pattern = input_api.re.compile(r'[A-Za-z0-9_]+::$')

    def FilterFile(affected_file):
        files_to_skip = (_EXCLUDED_PATHS + _TEST_CODE_EXCLUDED_PATHS +
                         input_api.DEFAULT_FILES_TO_SKIP)
        return input_api.FilterSourceFile(
            affected_file,
            files_to_check=file_inclusion_pattern,
            files_to_skip=files_to_skip)

    problems = []
    for f in input_api.AffectedSourceFiles(FilterFile):
        local_path = f.LocalPath()
        in_method_defn = False
        for line_number, line in f.ChangedContents():
            if (inclusion_pattern.search(line)
                    and not comment_pattern.search(line)
                    and not exclusion_pattern.search(line)
                    and not allowlist_pattern.search(line)
                    and not in_method_defn):
                problems.append('%s:%d\n    %s' %
                                (local_path, line_number, line.strip()))
            in_method_defn = method_defn_pattern.search(line)

    if problems:
        return [
            output_api.PresubmitPromptOrNotify(_TEST_ONLY_WARNING, problems)
        ]
    else:
        return []


def CheckNoProductionCodeUsingTestOnlyFunctionsJava(input_api, output_api):
    """This is a simplified version of
    CheckNoProductionCodeUsingTestOnlyFunctions for Java files.
    """
    javadoc_start_re = input_api.re.compile(r'^\s*/\*\*')
    javadoc_end_re = input_api.re.compile(r'^\s*\*/')
    name_pattern = r'ForTest(s|ing)?'
    # Describes an occurrence of "ForTest*" inside a // comment.
    comment_re = input_api.re.compile(r'//.*%s' % name_pattern)
    # Describes @VisibleForTesting(otherwise = VisibleForTesting.PROTECTED)
    annotation_re = input_api.re.compile(r'@VisibleForTesting\(')
    # Catch calls.
    inclusion_re = input_api.re.compile(r'(%s)\s*\(' % name_pattern)
    # Ignore definitions. (Comments are ignored separately.)
    exclusion_re = input_api.re.compile(r'(%s)[^;]+\{' % name_pattern)
    allowlist_re = input_api.re.compile(r'// IN-TEST$')

    problems = []
    sources = lambda x: input_api.FilterSourceFile(
        x,
        files_to_skip=(('(?i).*test', r'.*\/junit\/') + input_api.
                       DEFAULT_FILES_TO_SKIP),
        files_to_check=[r'.*\.java$'])
    for f in input_api.AffectedFiles(include_deletes=False,
                                     file_filter=sources):
        local_path = f.LocalPath()
        is_inside_javadoc = False
        for line_number, line in f.ChangedContents():
            if is_inside_javadoc and javadoc_end_re.search(line):
                is_inside_javadoc = False
            if not is_inside_javadoc and javadoc_start_re.search(line):
                is_inside_javadoc = True
            if is_inside_javadoc:
                continue
            if (inclusion_re.search(line) and not comment_re.search(line)
                    and not annotation_re.search(line)
                    and not allowlist_re.search(line)
                    and not exclusion_re.search(line)):
                problems.append('%s:%d\n    %s' %
                                (local_path, line_number, line.strip()))

    if problems:
        return [
            output_api.PresubmitPromptOrNotify(_TEST_ONLY_WARNING, problems)
        ]
    else:
        return []


def CheckNoIOStreamInHeaders(input_api, output_api):
    """Checks to make sure no .h files include <iostream>."""
    files = []
    pattern = input_api.re.compile(r'^#include\s*<iostream>',
                                   input_api.re.MULTILINE)
    for f in input_api.AffectedSourceFiles(input_api.FilterSourceFile):
        if not f.LocalPath().endswith('.h'):
            continue
        contents = input_api.ReadFile(f)
        if pattern.search(contents):
            files.append(f)

    if len(files):
        return [
            output_api.PresubmitError(
                'Do not #include <iostream> in header files, since it inserts static '
                'initialization into every file including the header. Instead, '
                '#include <ostream>. See http://crbug.com/94794', files)
        ]
    return []


def CheckNoStrCatRedefines(input_api, output_api):
    """Checks no windows headers with StrCat redefined are included directly."""
    files = []
    files_to_check = (r'.+%s' % _HEADER_EXTENSIONS,
                      r'.+%s' % _IMPLEMENTATION_EXTENSIONS)
    files_to_skip = (input_api.DEFAULT_FILES_TO_SKIP +
                     _NON_BASE_DEPENDENT_PATHS)
    sources_filter = lambda f: input_api.FilterSourceFile(
        f, files_to_check=files_to_check, files_to_skip=files_to_skip)

    pattern_deny = input_api.re.compile(
        r'^#include\s*[<"](shlwapi|atlbase|propvarutil|sphelper).h[">]',
        input_api.re.MULTILINE)
    pattern_allow = input_api.re.compile(
        r'^#include\s"base/win/windows_defines.inc"', input_api.re.MULTILINE)
    for f in input_api.AffectedSourceFiles(sources_filter):
        contents = input_api.ReadFile(f)
        if pattern_deny.search(
                contents) and not pattern_allow.search(contents):
            files.append(f.LocalPath())

    if len(files):
        return [
            output_api.PresubmitError(
                'Do not #include shlwapi.h, atlbase.h, propvarutil.h or sphelper.h '
                'directly since they pollute code with StrCat macro. Instead, '
                'include matching header from base/win. See http://crbug.com/856536',
                files)
        ]
    return []


def _CheckNoUNIT_TESTInSourceFiles(input_api, f):
    problems = []

    unit_test_macro = input_api.re.compile(
        '^\s*#.*(?:ifn?def\s+UNIT_TEST|defined\s*\(?\s*UNIT_TEST\s*\)?)(?:$|\s+)')
    for line_num, line in f.ChangedContents():
        if unit_test_macro.match(line):
            problems.append('    %s:%d' % (f.LocalPath(), line_num))

    return problems


def CheckNoUNIT_TESTInSourceFiles(input_api, output_api):
    """Checks to make sure no source files use UNIT_TEST."""
    problems = []
    for f in input_api.AffectedFiles():
        if (not f.LocalPath().endswith(('.cc', '.mm'))):
            continue
        problems.extend(
            _CheckNoUNIT_TESTInSourceFiles(input_api, f))

    if not problems:
        return []
    return [
        output_api.PresubmitPromptWarning('UNIT_TEST is only for headers.\n' +
                                          '\n'.join(problems))
    ]


def CheckNoDISABLETypoInTests(input_api, output_api):
    """Checks to prevent attempts to disable tests with DISABLE_ prefix.

    This test warns if somebody tries to disable a test with the DISABLE_ prefix
    instead of DISABLED_. To filter false positives, reports are only generated
    if a corresponding MAYBE_ line exists.
    """
    problems = []

    # The following two patterns are looked for in tandem - is a test labeled
    # as MAYBE_ followed by a DISABLE_ (instead of the correct DISABLED)
    maybe_pattern = input_api.re.compile(r'MAYBE_([a-zA-Z0-9_]+)')
    disable_pattern = input_api.re.compile(r'DISABLE_([a-zA-Z0-9_]+)')

    # This is for the case that a test is disabled on all platforms.
    full_disable_pattern = input_api.re.compile(
        r'^\s*TEST[^(]*\([a-zA-Z0-9_]+,\s*DISABLE_[a-zA-Z0-9_]+\)',
        input_api.re.MULTILINE)

    for f in input_api.AffectedFiles(include_deletes=False):
        if not 'test' in f.LocalPath() or not f.LocalPath().endswith('.cc'):
            continue

        # Search for MAYBE_, DISABLE_ pairs.
        disable_lines = {}  # Maps of test name to line number.
        maybe_lines = {}
        for line_num, line in f.ChangedContents():
            disable_match = disable_pattern.search(line)
            if disable_match:
                disable_lines[disable_match.group(1)] = line_num
            maybe_match = maybe_pattern.search(line)
            if maybe_match:
                maybe_lines[maybe_match.group(1)] = line_num

        # Search for DISABLE_ occurrences within a TEST() macro.
        disable_tests = set(disable_lines.keys())
        maybe_tests = set(maybe_lines.keys())
        for test in disable_tests.intersection(maybe_tests):
            problems.append('    %s:%d' % (f.LocalPath(), disable_lines[test]))

        contents = input_api.ReadFile(f)
        full_disable_match = full_disable_pattern.search(contents)
        if full_disable_match:
            problems.append('    %s' % f.LocalPath())

    if not problems:
        return []
    return [
        output_api.PresubmitPromptWarning(
            'Attempt to disable a test with DISABLE_ instead of DISABLED_?\n' +
            '\n'.join(problems))
    ]


def CheckForgettingMAYBEInTests(input_api, output_api):
    """Checks to make sure tests disabled conditionally are not missing a
    corresponding MAYBE_ prefix.
    """
    # Expect at least a lowercase character in the test name. This helps rule out
    # false positives with macros wrapping the actual tests name.
    define_maybe_pattern = input_api.re.compile(
        r'^\#define MAYBE_(?P<test_name>\w*[a-z]\w*)')
    # The test_maybe_pattern needs to handle all of these forms. The standard:
    #   IN_PROC_TEST_F(SyncTest, MAYBE_Start) {
    # With a wrapper macro around the test name:
    #   IN_PROC_TEST_F(SyncTest, E2E_ENABLED(MAYBE_Start)) {
    # And the odd-ball NACL_BROWSER_TEST_f format:
    #    NACL_BROWSER_TEST_F(NaClBrowserTest, SimpleLoad, {
    # The optional E2E_ENABLED-style is handled with (\w*\()?
    # The NACL_BROWSER_TEST_F pattern is handled by allowing a trailing comma or
    # trailing ')'.
    test_maybe_pattern = (
        r'^\s*\w*TEST[^(]*\(\s*\w+,\s*(\w*\()?MAYBE_{test_name}[\),]')
    suite_maybe_pattern = r'^\s*\w*TEST[^(]*\(\s*MAYBE_{test_name}[\),]'
    warnings = []

    # Read the entire files. We can't just read the affected lines, forgetting to
    # add MAYBE_ on a change would not show up otherwise.
    for f in input_api.AffectedFiles(include_deletes=False):
        if not 'test' in f.LocalPath() or not f.LocalPath().endswith('.cc'):
            continue
        contents = input_api.ReadFile(f)
        lines = contents.splitlines(True)
        current_position = 0
        warning_test_names = set()
        for line_num, line in enumerate(lines, start=1):
            current_position += len(line)
            maybe_match = define_maybe_pattern.search(line)
            if maybe_match:
                test_name = maybe_match.group('test_name')
                # Do not warn twice for the same test.
                if (test_name in warning_test_names):
                    continue
                warning_test_names.add(test_name)

                # Attempt to find the corresponding MAYBE_ test or suite, starting from
                # the current position.
                test_match = input_api.re.compile(
                    test_maybe_pattern.format(test_name=test_name),
                    input_api.re.MULTILINE).search(contents, current_position)
                suite_match = input_api.re.compile(
                    suite_maybe_pattern.format(test_name=test_name),
                    input_api.re.MULTILINE).search(contents, current_position)
                if not test_match and not suite_match:
                    warnings.append(
                        output_api.PresubmitPromptWarning(
                            '%s:%d found MAYBE_ defined without corresponding test %s'
                            % (f.LocalPath(), line_num, test_name)))
    return warnings


def CheckDCHECK_IS_ONHasBraces(input_api, output_api):
    """Checks to make sure DCHECK_IS_ON() does not skip the parentheses."""
    errors = []
    pattern = input_api.re.compile(r'\bDCHECK_IS_ON\b(?!\(\))',
                                   input_api.re.MULTILINE)
    for f in input_api.AffectedSourceFiles(input_api.FilterSourceFile):
        if (not f.LocalPath().endswith(('.cc', '.mm', '.h'))):
            continue
        for lnum, line in f.ChangedContents():
            if input_api.re.search(pattern, line):
                errors.append(
                    output_api.PresubmitError((
                        '%s:%d: Use of DCHECK_IS_ON() must be written as "#if '
                        + 'DCHECK_IS_ON()", not forgetting the parentheses.') %
                                              (f.LocalPath(), lnum)))
    return errors


# TODO(crbug/1138055): Reimplement CheckUmaHistogramChangesOnUpload check in a
# more reliable way. See
# https://chromium-review.googlesource.com/c/chromium/src/+/2500269


def CheckFlakyTestUsage(input_api, output_api):
    """Check that FlakyTest annotation is our own instead of the android one"""
    pattern = input_api.re.compile(r'import android.test.FlakyTest;')
    files = []
    for f in input_api.AffectedSourceFiles(input_api.FilterSourceFile):
        if f.LocalPath().endswith('Test.java'):
            if pattern.search(input_api.ReadFile(f)):
                files.append(f)
    if len(files):
        return [
            output_api.PresubmitError(
                'Use org.chromium.base.test.util.FlakyTest instead of '
                'android.test.FlakyTest', files)
        ]
    return []


def CheckNoDEPSGIT(input_api, output_api):
    """Make sure .DEPS.git is never modified manually."""
    if any(f.LocalPath().endswith('.DEPS.git')
           for f in input_api.AffectedFiles()):
        return [
            output_api.PresubmitError(
                'Never commit changes to .DEPS.git. This file is maintained by an\n'
                'automated system based on what\'s in DEPS and your changes will be\n'
                'overwritten.\n'
                'See https://sites.google.com/a/chromium.org/dev/developers/how-tos/'
                'get-the-code#Rolling_DEPS\n'
                'for more information')
        ]
    return []


def CheckCrosApiNeedBrowserTest(input_api, output_api):
    """Check new crosapi should add browser test."""
    has_new_crosapi = False
    has_browser_test = False
    for f in input_api.AffectedFiles():
        path = f.LocalPath()
        if (path.startswith('chromeos/crosapi/mojom') and
            _IsMojomFile(input_api, path) and f.Action() == 'A'):
            has_new_crosapi = True
        if path.endswith('browsertest.cc') or path.endswith('browser_test.cc'):
            has_browser_test = True
    if has_new_crosapi and not has_browser_test:
        return [
            output_api.PresubmitPromptWarning(
                'You are adding a new crosapi, but there is no file ends with '
                'browsertest.cc file being added or modified. It is important '
                'to add crosapi browser test coverage to avoid version '
                ' skew issues.\n'
                'Check //docs/lacros/test_instructions.md for more information.'
                )
        ]
    return []


def CheckValidHostsInDEPSOnUpload(input_api, output_api):
    """Checks that DEPS file deps are from allowed_hosts."""
    # Run only if DEPS file has been modified to annoy fewer bystanders.
    if all(f.LocalPath() != 'DEPS' for f in input_api.AffectedFiles()):
        return []
    # Outsource work to gclient verify
    try:
        gclient_path = input_api.os_path.join(input_api.PresubmitLocalPath(),
                                              'third_party', 'depot_tools',
                                              'gclient.py')
        input_api.subprocess.check_output(
            [input_api.python3_executable, gclient_path, 'verify'],
            stderr=input_api.subprocess.STDOUT)
        return []
    except input_api.subprocess.CalledProcessError as error:
        return [
            output_api.PresubmitError(
                'DEPS file must have only git dependencies.',
                long_text=error.output)
        ]


def _GetMessageForMatchingType(input_api, affected_file, line_number, line,
                               ban_rule):
    """Helper method for checking for banned constructs.

    Returns an string composed of the name of the file, the line number where the
    match has been found and the additional text passed as |message| in case the
    target type name matches the text inside the line passed as parameter.
    """
    result = []

    # Ignore comments about banned types.
    if input_api.re.search(r"^ *//", line):
        return result
    # A // nocheck comment will bypass this error.
    if line.endswith(" nocheck"):
        return result

    matched = False
    if ban_rule.pattern[0:1] == '/':
        regex = ban_rule.pattern[1:]
        if input_api.re.search(regex, line):
            matched = True
    elif ban_rule.pattern in line:
        matched = True

    if matched:
        result.append('    %s:%d:' % (affected_file.LocalPath(), line_number))
        for line in ban_rule.explanation:
            result.append('      %s' % line)

    return result


def CheckNoBannedFunctions(input_api, output_api):
    """Make sure that banned functions are not used."""
    warnings = []
    errors = []

    def IsExcludedFile(affected_file, excluded_paths):
        if not excluded_paths:
            return False

        local_path = affected_file.LocalPath()
        # Consistently use / as path separator to simplify the writing of regex
        # expressions.
        local_path = local_path.replace(input_api.os_path.sep, '/')
        for item in excluded_paths:
            if input_api.re.match(item, local_path):
                return True
        return False

    def IsIosObjcFile(affected_file):
        local_path = affected_file.LocalPath()
        if input_api.os_path.splitext(local_path)[-1] not in ('.mm', '.m',
                                                              '.h'):
            return False
        basename = input_api.os_path.basename(local_path)
        if 'ios' in basename.split('_'):
            return True
        for sep in (input_api.os_path.sep, input_api.os_path.altsep):
            if sep and 'ios' in local_path.split(sep):
                return True
        return False

    def CheckForMatch(affected_file, line_num: int, line: str,
                      ban_rule: BanRule):
        if IsExcludedFile(affected_file, ban_rule.excluded_paths):
            return

        problems = _GetMessageForMatchingType(input_api, f, line_num, line,
                                              ban_rule)
        if problems:
            if ban_rule.treat_as_error is not None and ban_rule.treat_as_error:
                errors.extend(problems)
            else:
                warnings.extend(problems)

    file_filter = lambda f: f.LocalPath().endswith(('.java'))
    for f in input_api.AffectedFiles(file_filter=file_filter):
        for line_num, line in f.ChangedContents():
            for ban_rule in _BANNED_JAVA_FUNCTIONS:
                CheckForMatch(f, line_num, line, ban_rule)

    file_filter = lambda f: f.LocalPath().endswith(('.js', '.ts'))
    for f in input_api.AffectedFiles(file_filter=file_filter):
        for line_num, line in f.ChangedContents():
            for ban_rule in _BANNED_JAVASCRIPT_FUNCTIONS:
                CheckForMatch(f, line_num, line, ban_rule)

    file_filter = lambda f: f.LocalPath().endswith(('.mm', '.m', '.h'))
    for f in input_api.AffectedFiles(file_filter=file_filter):
        for line_num, line in f.ChangedContents():
            for ban_rule in _BANNED_OBJC_FUNCTIONS:
                CheckForMatch(f, line_num, line, ban_rule)

    for f in input_api.AffectedFiles(file_filter=IsIosObjcFile):
        for line_num, line in f.ChangedContents():
            for ban_rule in _BANNED_IOS_OBJC_FUNCTIONS:
                CheckForMatch(f, line_num, line, ban_rule)

    egtest_filter = lambda f: f.LocalPath().endswith(('_egtest.mm'))
    for f in input_api.AffectedFiles(file_filter=egtest_filter):
        for line_num, line in f.ChangedContents():
            for ban_rule in _BANNED_IOS_EGTEST_FUNCTIONS:
                CheckForMatch(f, line_num, line, ban_rule)

    file_filter = lambda f: f.LocalPath().endswith(('.cc', '.mm', '.h'))
    for f in input_api.AffectedFiles(file_filter=file_filter):
        for line_num, line in f.ChangedContents():
            for ban_rule in _BANNED_CPP_FUNCTIONS:
                CheckForMatch(f, line_num, line, ban_rule)

    # As of 05/2024, iOS fully migrated ConsentLevel::kSync to kSignin, and
    # Android is in the process of preventing new users from entering kSync.
    # So the warning is restricted to those platforms.
    ios_pattern = input_api.re.compile('(^|[\W_])ios[\W_]')
    file_filter = lambda f: (f.LocalPath().endswith(('.cc', '.mm', '.h')) and
                             ('android' in f.LocalPath() or
                             # Simply checking for an 'ios' substring would
                             # catch unrelated cases, use a regex.
                              ios_pattern.search(f.LocalPath())))
    for f in input_api.AffectedFiles(file_filter=file_filter):
        for line_num, line in f.ChangedContents():
            for ban_rule in _DEPRECATED_SYNC_CONSENT_CPP_FUNCTIONS:
                CheckForMatch(f, line_num, line, ban_rule)

    file_filter = lambda f: f.LocalPath().endswith(('.java'))
    for f in input_api.AffectedFiles(file_filter=file_filter):
        for line_num, line in f.ChangedContents():
            for ban_rule in _DEPRECATED_SYNC_CONSENT_JAVA_FUNCTIONS:
                CheckForMatch(f, line_num, line, ban_rule)

    file_filter = lambda f: f.LocalPath().endswith(('.mojom'))
    for f in input_api.AffectedFiles(file_filter=file_filter):
        for line_num, line in f.ChangedContents():
            for ban_rule in _BANNED_MOJOM_PATTERNS:
                CheckForMatch(f, line_num, line, ban_rule)


    result = []
    if (warnings):
        result.append(
            output_api.PresubmitPromptWarning('Banned functions were used.\n' +
                                              '\n'.join(warnings)))
    if (errors):
        result.append(
            output_api.PresubmitError('Banned functions were used.\n' +
                                      '\n'.join(errors)))
    return result

def _CheckAndroidNoBannedImports(input_api, output_api):
    """Make sure that banned java imports are not used."""
    errors = []

    file_filter = lambda f: f.LocalPath().endswith(('.java'))
    for f in input_api.AffectedFiles(file_filter=file_filter):
        for line_num, line in f.ChangedContents():
            for ban_rule in _BANNED_JAVA_IMPORTS:
                # Consider merging this into the above function. There is no
                # real difference anymore other than helping with a little
                # bit of boilerplate text. Doing so means things like
                # `treat_as_error` will also be uniformly handled.
                problems = _GetMessageForMatchingType(input_api, f, line_num,
                                                      line, ban_rule)
                if problems:
                    errors.extend(problems)
    result = []
    if (errors):
        result.append(
            output_api.PresubmitError('Banned imports were used.\n' +
                                      '\n'.join(errors)))
    return result


def CheckNoPragmaOnce(input_api, output_api):
    """Make sure that banned functions are not used."""
    files = []
    pattern = input_api.re.compile(r'^#pragma\s+once', input_api.re.MULTILINE)
    for f in input_api.AffectedSourceFiles(input_api.FilterSourceFile):
        if not f.LocalPath().endswith('.h'):
            continue
        if f.LocalPath().endswith('com_imported_mstscax.h'):
            continue
        contents = input_api.ReadFile(f)
        if pattern.search(contents):
            files.append(f)

    if files:
        return [
            output_api.PresubmitError(
                'Do not use #pragma once in header files.\n'
                'See http://www.chromium.org/developers/coding-style#TOC-File-headers',
                files)
        ]
    return []


def CheckNoTrinaryTrueFalse(input_api, output_api):
    """Checks to make sure we don't introduce use of foo ? true : false."""
    problems = []
    pattern = input_api.re.compile(r'\?\s*(true|false)\s*:\s*(true|false)')
    for f in input_api.AffectedFiles():
        if not f.LocalPath().endswith(('.cc', '.h', '.inl', '.m', '.mm')):
            continue

        for line_num, line in f.ChangedContents():
            if pattern.match(line):
                problems.append('    %s:%d' % (f.LocalPath(), line_num))

    if not problems:
        return []
    return [
        output_api.PresubmitPromptWarning(
            'Please consider avoiding the "? true : false" pattern if possible.\n'
            + '\n'.join(problems))
    ]


def CheckUnwantedDependencies(input_api, output_api):
    """Runs checkdeps on #include and import statements added in this
    change. Breaking - rules is an error, breaking ! rules is a
    warning.
    """
    # Return early if no relevant file types were modified.
    for f in input_api.AffectedFiles():
        path = f.LocalPath()
        if (_IsCPlusPlusFile(input_api, path) or _IsProtoFile(input_api, path)
                or _IsJavaFile(input_api, path)):
            break
    else:
        return []

    import sys
    # We need to wait until we have an input_api object and use this
    # roundabout construct to import checkdeps because this file is
    # eval-ed and thus doesn't have __file__.
    original_sys_path = sys.path
    try:
        sys.path = sys.path + [
            input_api.os_path.join(input_api.PresubmitLocalPath(),
                                   'buildtools', 'checkdeps')
        ]
        import checkdeps
        from rules import Rule
    finally:
        # Restore sys.path to what it was before.
        sys.path = original_sys_path

    added_includes = []
    added_imports = []
    added_java_imports = []
    for f in input_api.AffectedFiles():
        if _IsCPlusPlusFile(input_api, f.LocalPath()):
            changed_lines = [line for _, line in f.ChangedContents()]
            added_includes.append([f.AbsoluteLocalPath(), changed_lines])
        elif _IsProtoFile(input_api, f.LocalPath()):
            changed_lines = [line for _, line in f.ChangedContents()]
            added_imports.append([f.AbsoluteLocalPath(), changed_lines])
        elif _IsJavaFile(input_api, f.LocalPath()):
            changed_lines = [line for _, line in f.ChangedContents()]
            added_java_imports.append([f.AbsoluteLocalPath(), changed_lines])

    deps_checker = checkdeps.DepsChecker(input_api.PresubmitLocalPath())

    error_descriptions = []
    warning_descriptions = []
    error_subjects = set()
    warning_subjects = set()

    for path, rule_type, rule_description in deps_checker.CheckAddedCppIncludes(
            added_includes):
        path = input_api.os_path.relpath(path, input_api.PresubmitLocalPath())
        description_with_path = '%s\n    %s' % (path, rule_description)
        if rule_type == Rule.DISALLOW:
            error_descriptions.append(description_with_path)
            error_subjects.add("#includes")
        else:
            warning_descriptions.append(description_with_path)
            warning_subjects.add("#includes")

    for path, rule_type, rule_description in deps_checker.CheckAddedProtoImports(
            added_imports):
        path = input_api.os_path.relpath(path, input_api.PresubmitLocalPath())
        description_with_path = '%s\n    %s' % (path, rule_description)
        if rule_type == Rule.DISALLOW:
            error_descriptions.append(description_with_path)
            error_subjects.add("imports")
        else:
            warning_descriptions.append(description_with_path)
            warning_subjects.add("imports")

    for path, rule_type, rule_description in deps_checker.CheckAddedJavaImports(
            added_java_imports, _JAVA_MULTIPLE_DEFINITION_EXCLUDED_PATHS):
        path = input_api.os_path.relpath(path, input_api.PresubmitLocalPath())
        description_with_path = '%s\n    %s' % (path, rule_description)
        if rule_type == Rule.DISALLOW:
            error_descriptions.append(description_with_path)
            error_subjects.add("imports")
        else:
            warning_descriptions.append(description_with_path)
            warning_subjects.add("imports")

    results = []
    if error_descriptions:
        results.append(
            output_api.PresubmitError(
                'You added one or more %s that violate checkdeps rules.' %
                " and ".join(error_subjects), error_descriptions))
    if warning_descriptions:
        results.append(
            output_api.PresubmitPromptOrNotify(
                'You added one or more %s of files that are temporarily\n'
                'allowed but being removed. Can you avoid introducing the\n'
                '%s? See relevant DEPS file(s) for details and contacts.' %
                (" and ".join(warning_subjects), "/".join(warning_subjects)),
                warning_descriptions))
    return results


def CheckFilePermissions(input_api, output_api):
    """Check that all files have their permissions properly set."""
    if input_api.platform == 'win32':
        return []
    checkperms_tool = input_api.os_path.join(input_api.PresubmitLocalPath(),
                                             'tools', 'checkperms',
                                             'checkperms.py')
    args = [
        input_api.python3_executable, checkperms_tool, '--root',
        input_api.change.RepositoryRoot()
    ]
    with input_api.CreateTemporaryFile() as file_list:
        for f in input_api.AffectedFiles():
            # checkperms.py file/directory arguments must be relative to the
            # repository.
            file_list.write((f.LocalPath() + '\n').encode('utf8'))
        file_list.close()
        args += ['--file-list', file_list.name]
        try:
            input_api.subprocess.check_output(args)
            return []
        except input_api.subprocess.CalledProcessError as error:
            return [
                output_api.PresubmitError('checkperms.py failed:',
                                          long_text=error.output.decode(
                                              'utf-8', 'ignore'))
            ]


def CheckNoAuraWindowPropertyHInHeaders(input_api, output_api):
    """Makes sure we don't include ui/aura/window_property.h
    in header files.
    """
    pattern = input_api.re.compile(r'^#include\s*"ui/aura/window_property.h"')
    errors = []
    for f in input_api.AffectedFiles():
        if not f.LocalPath().endswith('.h'):
            continue
        for line_num, line in f.ChangedContents():
            if pattern.match(line):
                errors.append('    %s:%d' % (f.LocalPath(), line_num))

    results = []
    if errors:
        results.append(
            output_api.PresubmitError(
                'Header files should not include ui/aura/window_property.h',
                errors))
    return results


def CheckNoInternalHeapIncludes(input_api, output_api):
    """Makes sure we don't include any headers from
    third_party/blink/renderer/platform/heap/impl or
    third_party/blink/renderer/platform/heap/v8_wrapper from files outside of
    third_party/blink/renderer/platform/heap
    """
    impl_pattern = input_api.re.compile(
        r'^\s*#include\s*"third_party/blink/renderer/platform/heap/impl/.*"')
    v8_wrapper_pattern = input_api.re.compile(
        r'^\s*#include\s*"third_party/blink/renderer/platform/heap/v8_wrapper/.*"'
    )
    # Consistently use / as path separator to simplify the writing of regex
    # expressions.
    file_filter = lambda f: not input_api.re.match(
        r"^third_party/blink/renderer/platform/heap/.*",
        f.LocalPath().replace(input_api.os_path.sep, '/'))
    errors = []

    for f in input_api.AffectedFiles(file_filter=file_filter):
        for line_num, line in f.ChangedContents():
            if impl_pattern.match(line) or v8_wrapper_pattern.match(line):
                errors.append('    %s:%d' % (f.LocalPath(), line_num))

    results = []
    if errors:
        results.append(
            output_api.PresubmitError(
                'Do not include files from third_party/blink/renderer/platform/heap/impl'
                ' or third_party/blink/renderer/platform/heap/v8_wrapper. Use the '
                'relevant counterparts from third_party/blink/renderer/platform/heap',
                errors))
    return results


def _CheckForVersionControlConflictsInFile(input_api, f):
    pattern = input_api.re.compile('^(?:<<<<<<<|>>>>>>>) |^=======$')
    errors = []
    for line_num, line in f.ChangedContents():
        if f.LocalPath().endswith(('.md', '.rst', '.txt')):
            # First-level headers in markdown look a lot like version control
            # conflict markers. http://daringfireball.net/projects/markdown/basics
            continue
        if pattern.match(line):
            errors.append('    %s:%d %s' % (f.LocalPath(), line_num, line))
    return errors


def CheckForVersionControlConflicts(input_api, output_api):
    """Usually this is not intentional and will cause a compile failure."""
    errors = []
    for f in input_api.AffectedFiles():
        errors.extend(_CheckForVersionControlConflictsInFile(input_api, f))

    results = []
    if errors:
        results.append(
            output_api.PresubmitError(
                'Version control conflict markers found, please resolve.',
                errors))
    return results


def CheckGoogleSupportAnswerUrlOnUpload(input_api, output_api):
    pattern = input_api.re.compile('support\.google\.com\/chrome.*/answer')
    errors = []
    for f in input_api.AffectedFiles():
        for line_num, line in f.ChangedContents():
            if pattern.search(line):
                errors.append('    %s:%d %s' % (f.LocalPath(), line_num, line))

    results = []
    if errors:
        results.append(
            output_api.PresubmitPromptWarning(
                'Found Google support URL addressed by answer number. Please replace '
                'with a p= identifier instead. See crbug.com/679462\n',
                errors))
    return results


def CheckHardcodedGoogleHostsInLowerLayers(input_api, output_api):
    def FilterFile(affected_file):
        """Filter function for use with input_api.AffectedSourceFiles,
        below.  This filters out everything except non-test files from
        top-level directories that generally speaking should not hard-code
        service URLs (e.g. src/android_webview/, src/content/ and others).
        """
        return input_api.FilterSourceFile(
            affected_file,
            files_to_check=[r'^(android_webview|base|content|net)/.*'],
            files_to_skip=(_EXCLUDED_PATHS + _TEST_CODE_EXCLUDED_PATHS +
                           input_api.DEFAULT_FILES_TO_SKIP))

    base_pattern = ('"[^"]*(google|googleapis|googlezip|googledrive|appspot)'
                    '\.(com|net)[^"]*"')
    comment_pattern = input_api.re.compile('//.*%s' % base_pattern)
    pattern = input_api.re.compile(base_pattern)
    problems = []  # items are (filename, line_number, line)
    for f in input_api.AffectedSourceFiles(FilterFile):
        for line_num, line in f.ChangedContents():
            if not comment_pattern.search(line) and pattern.search(line):
                problems.append((f.LocalPath(), line_num, line))

    if problems:
        return [
            output_api.PresubmitPromptOrNotify(
                'Most layers below src/chrome/ should not hardcode service URLs.\n'
                'Are you sure this is correct?', [
                    '  %s:%d:  %s' % (problem[0], problem[1], problem[2])
                    for problem in problems
                ])
        ]
    else:
        return []


def CheckChromeOsSyncedPrefRegistration(input_api, output_api):
    """Warns if Chrome OS C++ files register syncable prefs as browser prefs."""

    def FileFilter(affected_file):
        """Includes directories known to be Chrome OS only."""
        return input_api.FilterSourceFile(
            affected_file,
            files_to_check=(
                '^ash/',
                '^chromeos/',  # Top-level src/chromeos.
                '.*/chromeos/',  # Any path component.
                '^components/arc',
                '^components/exo'),
            files_to_skip=(input_api.DEFAULT_FILES_TO_SKIP))

    prefs = []
    priority_prefs = []
    for f in input_api.AffectedFiles(file_filter=FileFilter):
        for line_num, line in f.ChangedContents():
            if input_api.re.search('PrefRegistrySyncable::SYNCABLE_PREF',
                                   line):
                prefs.append('    %s:%d:' % (f.LocalPath(), line_num))
                prefs.append('      %s' % line)
            if input_api.re.search(
                    'PrefRegistrySyncable::SYNCABLE_PRIORITY_PREF', line):
                priority_prefs.append('    %s:%d' % (f.LocalPath(), line_num))
                priority_prefs.append('      %s' % line)

    results = []
    if (prefs):
        results.append(
            output_api.PresubmitPromptWarning(
                'Preferences were registered as SYNCABLE_PREF and will be controlled '
                'by browser sync settings. If these prefs should be controlled by OS '
                'sync settings use SYNCABLE_OS_PREF instead.\n' +
                '\n'.join(prefs)))
    if (priority_prefs):
        results.append(
            output_api.PresubmitPromptWarning(
                'Preferences were registered as SYNCABLE_PRIORITY_PREF and will be '
                'controlled by browser sync settings. If these prefs should be '
                'controlled by OS sync settings use SYNCABLE_OS_PRIORITY_PREF '
                'instead.\n' + '\n'.join(prefs)))
    return results


def CheckNoAbbreviationInPngFileName(input_api, output_api):
    """Makes sure there are no abbreviations in the name of PNG files.
    The native_client_sdk directory is excluded because it has auto-generated PNG
    files for documentation.
    """
    errors = []
    files_to_check = [r'.*\.png$']
    files_to_skip = [r'^native_client_sdk/',
                     r'^services/test/',
                     r'^third_party/blink/web_tests/',
                    ]
    file_filter = lambda f: input_api.FilterSourceFile(
        f, files_to_check=files_to_check, files_to_skip=files_to_skip)
    abbreviation = input_api.re.compile('.+_[a-z]\.png|.+_[a-z]_.*\.png')
    for f in input_api.AffectedFiles(include_deletes=False,
                                     file_filter=file_filter):
        file_name = input_api.os_path.split(f.LocalPath())[1]
        if abbreviation.search(file_name):
            errors.append('    %s' % f.LocalPath())

    results = []
    if errors:
        results.append(
            output_api.PresubmitError(
                'The name of PNG files should not have abbreviations. \n'
                'Use _hover.png, _center.png, instead of _h.png, _c.png.\n'
                'Contact oshima@chromium.org if you have questions.', errors))
    return results

def CheckNoProductIconsAddedToPublicRepo(input_api, output_api):
    """Heuristically identifies product icons based on their file name and reminds
    contributors not to add them to the Chromium repository.
    """
    errors = []
    files_to_check = [r'.*google.*\.png$|.*google.*\.svg$|.*google.*\.icon$']
    file_filter = lambda f: input_api.FilterSourceFile(
        f, files_to_check=files_to_check)
    for f in input_api.AffectedFiles(include_deletes=False,
                                     file_filter=file_filter):
        errors.append('    %s' % f.LocalPath())

    results = []
    if errors:
        # Give warnings instead of errors on presubmit --all and presubmit
        # --files.
        message_type = (output_api.PresubmitNotifyResult if input_api.no_diffs
                        else output_api.PresubmitError)
        results.append(
            message_type(
                'Trademarked images should not be added to the public repo. '
                'See crbug.com/944754', errors))
    return results


def _ExtractAddRulesFromParsedDeps(parsed_deps):
    """Extract the rules that add dependencies from a parsed DEPS file.

    Args:
      parsed_deps: the locals dictionary from evaluating the DEPS file."""
    add_rules = set()
    add_rules.update([
        rule[1:] for rule in parsed_deps.get('include_rules', [])
        if rule.startswith('+') or rule.startswith('!')
    ])
    for _, rules in parsed_deps.get('specific_include_rules', {}).items():
        add_rules.update([
            rule[1:] for rule in rules
            if rule.startswith('+') or rule.startswith('!')
        ])
    return add_rules


def _ParseDeps(contents):
    """Simple helper for parsing DEPS files."""

    # Stubs for handling special syntax in the root DEPS file.
    class _VarImpl:
        def __init__(self, local_scope):
            self._local_scope = local_scope

        def Lookup(self, var_name):
            """Implements the Var syntax."""
            try:
                return self._local_scope['vars'][var_name]
            except KeyError:
                raise Exception('Var is not defined: %s' % var_name)

    local_scope = {}
    global_scope = {
        'Var': _VarImpl(local_scope).Lookup,
        'Str': str,
    }

    exec(contents, global_scope, local_scope)
    return local_scope


def _CalculateAddedDeps(os_path, old_contents, new_contents):
    """Helper method for CheckAddedDepsHaveTargetApprovals. Returns
    a set of DEPS entries that we should look up.

    For a directory (rather than a specific filename) we fake a path to
    a specific filename by adding /DEPS. This is chosen as a file that
    will seldom or never be subject to per-file include_rules.
    """
    # We ignore deps entries on auto-generated directories.
    AUTO_GENERATED_DIRS = ['grit', 'jni']

    old_deps = _ExtractAddRulesFromParsedDeps(_ParseDeps(old_contents))
    new_deps = _ExtractAddRulesFromParsedDeps(_ParseDeps(new_contents))

    added_deps = new_deps.difference(old_deps)

    results = set()
    for added_dep in added_deps:
        if added_dep.split('/')[0] in AUTO_GENERATED_DIRS:
            continue
        # Assume that a rule that ends in .h is a rule for a specific file.
        if added_dep.endswith('.h'):
            results.add(added_dep)
        else:
            results.add(os_path.join(added_dep, 'DEPS'))
    return results

def CheckForNewDEPSDownloadFromGoogleStorageHooks(input_api, output_api):
    """Checks that there are no new download_from_google_storage hooks"""
    for f in input_api.AffectedFiles(include_deletes=False):
        if f.LocalPath() == 'DEPS':
            old_hooks = _ParseDeps('\n'.join(f.OldContents()))['hooks']
            new_hooks = _ParseDeps('\n'.join(f.NewContents()))['hooks']
            old_name_to_hook = {hook['name']: hook for hook in old_hooks}
            new_name_to_hook = {hook['name']: hook for hook in new_hooks}
            added_hook_names = set(new_name_to_hook.keys()) - set(
                old_name_to_hook.keys())
            if not added_hook_names:
                return []
            new_download_from_google_storage_hooks = []
            for new_hook in added_hook_names:
                hook = new_name_to_hook[new_hook]
                action_cmd = hook['action']
                if any('download_from_google_storage' in arg
                        for arg in action_cmd):
                    new_download_from_google_storage_hooks.append(new_hook)
            if new_download_from_google_storage_hooks:
                return [
                    output_api.PresubmitError(
                        'Please do not add new download_from_google_storage '
                        'hooks. Instead, add a `gcs` dep_type entry to `deps`. '
                        'See https://chromium.googlesource.com/chromium/src.git'
                        '/+/refs/heads/main/docs/gcs_dependencies.md for more '
                        'info. Added hooks:',
                        items=new_download_from_google_storage_hooks)
                ]
    return []


def CheckEachPerfettoTestDataFileHasDepsEntry(input_api, output_api):
    test_data_filter = lambda f: input_api.FilterSourceFile(
        f, files_to_check=[r'^base/tracing/test/data_sha256/.*\.sha256'])
    if not any(input_api.AffectedFiles(file_filter=test_data_filter)):
        return []

    # Find DEPS entry
    deps_entry = []
    old_deps_entry = []
    for f in input_api.AffectedFiles(include_deletes=False):
        if f.LocalPath() == 'DEPS':
            new_deps = _ParseDeps('\n'.join(f.NewContents()))['deps']
            deps_entry = new_deps['src/base/tracing/test/data']
            old_deps = _ParseDeps('\n'.join(f.OldContents()))['deps']
            old_deps_entry = old_deps['src/base/tracing/test/data']
    if not deps_entry:
        # TODO(312895063):Add back error when .sha256 files have been moved.
        return [output_api.PresubmitError(
            'You must update the DEPS file when you update a '
            '.sha256 file in base/tracing/test/data_sha256'
        )]

    output = []
    for f in input_api.AffectedFiles(file_filter=test_data_filter):
        objects = deps_entry['objects']
        if not f.NewContents():
            # Deleted file so check that DEPS entry removed
            sha256_from_file = f.OldContents()[0]
            object_entry = next(
                (item for item in objects if item["sha256sum"] == sha256_from_file),
                None)
            old_entry = next(
                (item for item in old_deps_entry['objects'] if item["sha256sum"] == sha256_from_file),
                None)
            if object_entry:
                # Allow renaming of objects with the same hash
                if object_entry['object_name'] != old_entry['object_name']:
                    continue
                output.append(output_api.PresubmitError(
                    'You deleted %s so you must also remove the corresponding DEPS entry.'
                    % f.LocalPath()
                ))
            continue

        sha256_from_file = f.NewContents()[0]
        object_entry = next(
            (item for item in objects if item["sha256sum"] == sha256_from_file),
            None)
        if not object_entry:
            output.append(output_api.PresubmitError(
                'No corresponding DEPS entry found for %s. '
                'Run `base/tracing/test/test_data.py get_deps --filepath %s` '
                'to generate the DEPS entry.'
                % (f.LocalPath(), f.LocalPath())
            ))

    if output:
        output.append(output_api.PresubmitError(
            'The DEPS entry for `src/base/tracing/test/data` in the DEPS file has not been '
            'updated properly. Run `base/tracing/test/test_data.py get_all_deps` to see what '
            'the DEPS entry should look like.'
        ))
    return output


def CheckAddedDepsHaveTargetApprovals(input_api, output_api):
    """When a dependency prefixed with + is added to a DEPS file, we
    want to make sure that the change is reviewed by an OWNER of the
    target file or directory, to avoid layering violations from being
    introduced. This check verifies that this happens.
    """
    # We rely on Gerrit's code-owners to check approvals.
    # input_api.gerrit is always set for Chromium, but other projects
    # might not use Gerrit.
    if not input_api.gerrit or input_api.no_diffs:
        return []
    if 'PRESUBMIT_SKIP_NETWORK' in input_api.environ:
        return []
    try:
        if (input_api.change.issue and
                input_api.gerrit.IsOwnersOverrideApproved(
                input_api.change.issue)):
            # Skip OWNERS check when Owners-Override label is approved. This is
            # intended for global owners, trusted bots, and on-call sheriffs.
            # Review is still required for these changes.
            return []
    except Exception as e:
        return [output_api.PresubmitPromptWarning(
                'Failed to retrieve owner override status - %s' % str(e))]

    virtual_depended_on_files = set()

    # Consistently use / as path separator to simplify the writing of regex
    # expressions.
    file_filter = lambda f: not input_api.re.match(
        r"^third_party/blink/.*",
        f.LocalPath().replace(input_api.os_path.sep, '/'))
    for f in input_api.AffectedFiles(include_deletes=False,
                                     file_filter=file_filter):
        filename = input_api.os_path.basename(f.LocalPath())
        if filename == 'DEPS':
            virtual_depended_on_files.update(
                _CalculateAddedDeps(input_api.os_path,
                                    '\n'.join(f.OldContents()),
                                    '\n'.join(f.NewContents())))

    if not virtual_depended_on_files:
        return []

    if input_api.is_committing:
        if input_api.tbr:
            return [
                output_api.PresubmitNotifyResult(
                    '--tbr was specified, skipping OWNERS check for DEPS additions'
                )
            ]
        # TODO(dcheng): Make this generate an error on dry runs if the reviewer
        # is not added, to prevent review serialization.
        if input_api.dry_run:
            return [
                output_api.PresubmitNotifyResult(
                    'This is a dry run, skipping OWNERS check for DEPS additions'
                )
            ]
        if not input_api.change.issue:
            return [
                output_api.PresubmitError(
                    "DEPS approval by OWNERS check failed: this change has "
                    "no change number, so we can't check it for approvals.")
            ]
        output = output_api.PresubmitError
    else:
        output = output_api.PresubmitNotifyResult

    owner_email, reviewers = (
        input_api.canned_checks.GetCodereviewOwnerAndReviewers(
            input_api, None, approval_needed=input_api.is_committing))

    owner_email = owner_email or input_api.change.author_email

    approval_status = input_api.owners_client.GetFilesApprovalStatus(
        virtual_depended_on_files, reviewers.union([owner_email]), [])
    missing_files = [
        f for f in virtual_depended_on_files
        if approval_status[f] != input_api.owners_client.APPROVED
    ]

    # We strip the /DEPS part that was added by
    # _FilesToCheckForIncomingDeps to fake a path to a file in a
    # directory.
    def StripDeps(path):
        start_deps = path.rfind('/DEPS')
        if start_deps != -1:
            return path[:start_deps]
        else:
            return path

    submodule_paths = set(input_api.ListSubmodules())
    def is_from_submodules(path, submodule_paths):
        path = input_api.os_path.normpath(path)
        while path:
            if path in submodule_paths:
                return True

            # All deps should be a relative path from the checkout.
            # i.e., shouldn't start with "/" or "c:\", for example.
            #
            # That said, this is to prevent an infinite loop, just in case
            # an input dep path starts with "/", because
            # os.path.dirname("/") => "/"
            parent = input_api.os_path.dirname(path)
            if parent == path:
                break
            path = parent

        return False

    unapproved_dependencies = [
        "'+%s'," % StripDeps(path) for path in missing_files
        # if a newly added dep is from a submodule, it becomes trickier
        # to get suggested owners, especially it is from a different host.
        #
        # skip the review enforcement for cross-repo deps.
        if not is_from_submodules(path, submodule_paths)
    ]

    if unapproved_dependencies:
        output_list = [
            output(
                'You need LGTM from owners of depends-on paths in DEPS that were '
                'modified in this CL:\n    %s' %
                '\n    '.join(sorted(unapproved_dependencies)))
        ]
        suggested_owners = input_api.owners_client.SuggestOwners(
            missing_files, exclude=[owner_email])
        output_list.append(
            output('Suggested missing target path OWNERS:\n    %s' %
                   '\n    '.join(suggested_owners or [])))
        return output_list

    return []


# TODO: add unit tests.
def CheckSpamLogging(input_api, output_api):
    file_inclusion_pattern = [r'.+%s' % _IMPLEMENTATION_EXTENSIONS]
    files_to_skip = (
        _EXCLUDED_PATHS + _TEST_CODE_EXCLUDED_PATHS +
        input_api.DEFAULT_FILES_TO_SKIP + (
            r"^base/fuchsia/scoped_fx_logger\.cc$",
            r"^base/logging\.h$",
            r"^base/logging\.cc$",
            r"^base/task/thread_pool/task_tracker\.cc$",
            r"^chrome/app/chrome_main_delegate\.cc$",
            r"^chrome/browser/ash/arc/enterprise/cert_store/arc_cert_installer\.cc$",
            r"^chrome/browser/ash/policy/remote_commands/user_command_arc_job\.cc$",
            r"^chrome/browser/chrome_browser_main\.cc$",
            r"^chrome/browser/ui/startup/startup_browser_creator\.cc$",
            r"^chrome/browser/browser_switcher/bho/.*",
            r"^chrome/browser/diagnostics/diagnostics_writer\.cc$",
            r"^chrome/chrome_elf/dll_hash/dll_hash_main\.cc$",
            r"^chrome/installer/setup/.*",
            # crdmg runs as a separate binary which intentionally does
            # not depend on base logging.
            r"^chrome/utility/safe_browsing/mac/crdmg\.cc$",
            r"^chromecast/",
            r"^components/cast",
            r"^components/media_control/renderer/media_playback_options\.cc$",
            r"^components/policy/core/common/policy_logger\.cc$",
            r"^components/viz/service/display/"
            r"overlay_strategy_underlay_cast\.cc$",
            r"^components/zucchini/.*",
            # TODO(peter): Remove exception. https://crbug.com/534537
            r"^content/browser/notifications/"
            r"notification_event_dispatcher_impl\.cc$",
            r"^content/common/gpu/client/gl_helper_benchmark\.cc$",
            r"^courgette/courgette_minimal_tool\.cc$",
            r"^courgette/courgette_tool\.cc$",
            r"^extensions/renderer/logging_native_handler\.cc$",
            r"^fuchsia_web/common/init_logging\.cc$",
            r"^fuchsia_web/runners/common/web_component\.cc$",
            r"^fuchsia_web/shell/.*\.cc$",
            r"^headless/app/headless_shell\.cc$",
            r"^ipc/ipc_logging\.cc$",
            r"^native_client_sdk/",
            r"^remoting/base/logging\.h$",
            r"^remoting/host/.*",
            r"^sandbox/linux/.*",
            r"^services/webnn/tflite/graph_impl_tflite\.cc$",
            r"^services/webnn/coreml/graph_impl_coreml\.mm$",
            r"^storage/browser/file_system/dump_file_system\.cc$",
            r"^testing/perf/",
            r"^tools/",
            r"^ui/base/resource/data_pack\.cc$",
            r"^ui/aura/bench/bench_main\.cc$",
            r"^ui/ozone/platform/cast/",
            r"^ui/base/x/xwmstartupcheck/"
            r"xwmstartupcheck\.cc$"))
    source_file_filter = lambda x: input_api.FilterSourceFile(
        x, files_to_check=file_inclusion_pattern, files_to_skip=files_to_skip)

    log_info = set([])
    printf = set([])

    for f in input_api.AffectedSourceFiles(source_file_filter):
        for _, line in f.ChangedContents():
            if input_api.re.search(r"\bD?LOG\s*\(\s*INFO\s*\)", line):
                log_info.add(f.LocalPath())
            elif input_api.re.search(r"\bD?LOG_IF\s*\(\s*INFO\s*,", line):
                log_info.add(f.LocalPath())

            if input_api.re.search(r"\bprintf\(", line):
                printf.add(f.LocalPath())
            elif input_api.re.search(r"\bfprintf\((stdout|stderr)", line):
                printf.add(f.LocalPath())

    if log_info:
        return [
            output_api.PresubmitError(
                'These files spam the console log with LOG(INFO):',
                items=log_info)
        ]
    if printf:
        return [
            output_api.PresubmitError(
                'These files spam the console log with printf/fprintf:',
                items=printf)
        ]
    return []


def CheckForAnonymousVariables(input_api, output_api):
    """These types are all expected to hold locks while in scope and
    so should never be anonymous (which causes them to be immediately
    destroyed)."""
    they_who_must_be_named = [
        'base::AutoLock',
        'base::AutoReset',
        'base::AutoUnlock',
        'SkAutoAlphaRestore',
        'SkAutoBitmapShaderInstall',
        'SkAutoBlitterChoose',
        'SkAutoBounderCommit',
        'SkAutoCallProc',
        'SkAutoCanvasRestore',
        'SkAutoCommentBlock',
        'SkAutoDescriptor',
        'SkAutoDisableDirectionCheck',
        'SkAutoDisableOvalCheck',
        'SkAutoFree',
        'SkAutoGlyphCache',
        'SkAutoHDC',
        'SkAutoLockColors',
        'SkAutoLockPixels',
        'SkAutoMalloc',
        'SkAutoMaskFreeImage',
        'SkAutoMutexAcquire',
        'SkAutoPathBoundsUpdate',
        'SkAutoPDFRelease',
        'SkAutoRasterClipValidate',
        'SkAutoRef',
        'SkAutoTime',
        'SkAutoTrace',
        'SkAutoUnref',
    ]
    anonymous = r'(%s)\s*[({]' % '|'.join(they_who_must_be_named)
    # bad: base::AutoLock(lock.get());
    # not bad: base::AutoLock lock(lock.get());
    bad_pattern = input_api.re.compile(anonymous)
    # good: new base::AutoLock(lock.get())
    good_pattern = input_api.re.compile(r'\bnew\s*' + anonymous)
    errors = []

    for f in input_api.AffectedFiles():
        if not f.LocalPath().endswith(('.cc', '.h', '.inl', '.m', '.mm')):
            continue
        for linenum, line in f.ChangedContents():
            if bad_pattern.search(line) and not good_pattern.search(line):
                errors.append('%s:%d' % (f.LocalPath(), linenum))

    if errors:
        return [
            output_api.PresubmitError(
                'These lines create anonymous variables that need to be named:',
                items=errors)
        ]
    return []


def CheckUniquePtrOnUpload(input_api, output_api):
    # Returns whether |template_str| is of the form <T, U...> for some types T
    # and U, or is invalid due to mismatched angle bracket pairs. Assumes that
    # |template_str| is already in the form <...>.
    def HasMoreThanOneArgOrInvalid(template_str):
        # Level of <...> nesting.
        nesting = 0
        for c in template_str:
            if c == '<':
                nesting += 1
            elif c == '>':
                nesting -= 1
            elif c == ',' and nesting == 1:
                return True
        if nesting != 0:
            # Invalid.
            return True
        return False

    file_inclusion_pattern = [r'.+%s' % _IMPLEMENTATION_EXTENSIONS]
    sources = lambda affected_file: input_api.FilterSourceFile(
        affected_file,
        files_to_skip=(_EXCLUDED_PATHS + _TEST_CODE_EXCLUDED_PATHS + input_api.
                       DEFAULT_FILES_TO_SKIP),
        files_to_check=file_inclusion_pattern)

    # Pattern to capture a single "<...>" block of template arguments. It can
    # handle linearly nested blocks, such as "<std::vector<std::set<T>>>", but
    # cannot handle branching structures, such as "<pair<set<T>,set<U>>". The
    # latter would likely require counting that < and > match, which is not
    # expressible in regular languages. Should the need arise, one can introduce
    # limited counting (matching up to a total number of nesting depth), which
    # should cover all practical cases for already a low nesting limit.
    template_arg_pattern = (
        r'<[^>]*'  # Opening block of <.
        r'>([^<]*>)?')  # Closing block of >.
    # Prefix expressing that whatever follows is not already inside a <...>
    # block.
    not_inside_template_arg_pattern = r'(^|[^<,\s]\s*)'
    null_construct_pattern = input_api.re.compile(
        not_inside_template_arg_pattern + r'\bstd::unique_ptr' +
        template_arg_pattern + r'\(\)')

    # Same as template_arg_pattern, but excluding type arrays, e.g., <T[]>.
    template_arg_no_array_pattern = (
        r'<[^>]*[^]]'  # Opening block of <.
        r'>([^(<]*[^]]>)?')  # Closing block of >.
    # Prefix saying that what follows is the start of an expression.
    start_of_expr_pattern = r'(=|\breturn|^)\s*'
    # Suffix saying that what follows are call parentheses with a non-empty list
    # of arguments.
    nonempty_arg_list_pattern = r'\(([^)]|$)'
    # Put the template argument into a capture group for deeper examination later.
    return_construct_pattern = input_api.re.compile(
        start_of_expr_pattern + r'std::unique_ptr' + '(?P<template_arg>' +
        template_arg_no_array_pattern + ')' + nonempty_arg_list_pattern)

    problems_constructor = []
    problems_nullptr = []
    for f in input_api.AffectedSourceFiles(sources):
        for line_number, line in f.ChangedContents():
            # Disallow:
            # return std::unique_ptr<T>(foo);
            # bar = std::unique_ptr<T>(foo);
            # But allow:
            # return std::unique_ptr<T[]>(foo);
            # bar = std::unique_ptr<T[]>(foo);
            # And also allow cases when the second template argument is present. Those
            # cases cannot be handled by std::make_unique:
            # return std::unique_ptr<T, U>(foo);
            # bar = std::unique_ptr<T, U>(foo);
            local_path = f.LocalPath()
            return_construct_result = return_construct_pattern.search(line)
            if return_construct_result and not HasMoreThanOneArgOrInvalid(
                    return_construct_result.group('template_arg')):
                problems_constructor.append(
                    '%s:%d\n    %s' % (local_path, line_number, line.strip()))
            # Disallow:
            # std::unique_ptr<T>()
            if null_construct_pattern.search(line):
                problems_nullptr.append(
                    '%s:%d\n    %s' % (local_path, line_number, line.strip()))

    errors = []
    if problems_nullptr:
        errors.append(
            output_api.PresubmitPromptWarning(
                'The following files use std::unique_ptr<T>(). Use nullptr instead.',
                problems_nullptr))
    if problems_constructor:
        errors.append(
            output_api.PresubmitError(
                'The following files use explicit std::unique_ptr constructor. '
                'Use std::make_unique<T>() instead, or use base::WrapUnique if '
                'std::make_unique is not an option.', problems_constructor))
    return errors


def CheckUserActionUpdate(input_api, output_api):
    """Checks if any new user action has been added."""
    if any('actions.xml' == input_api.os_path.basename(f)
           for f in input_api.LocalPaths()):
        # If actions.xml is already included in the changelist, the PRESUBMIT
        # for actions.xml will do a more complete presubmit check.
        return []

    file_inclusion_pattern = [r'.*\.(cc|mm)$']
    files_to_skip = (_EXCLUDED_PATHS + _TEST_CODE_EXCLUDED_PATHS +
                     input_api.DEFAULT_FILES_TO_SKIP)
    file_filter = lambda f: input_api.FilterSourceFile(
        f, files_to_check=file_inclusion_pattern, files_to_skip=files_to_skip)

    action_re = r'[^a-zA-Z]UserMetricsAction\("([^"]*)'
    current_actions = None
    for f in input_api.AffectedFiles(file_filter=file_filter):
        for line_num, line in f.ChangedContents():
            match = input_api.re.search(action_re, line)
            if match:
                # Loads contents in tools/metrics/actions/actions.xml to memory. It's
                # loaded only once.
                if not current_actions:
                    with open('tools/metrics/actions/actions.xml',
                              encoding='utf-8') as actions_f:
                        current_actions = actions_f.read()
                # Search for the matched user action name in |current_actions|.
                for action_name in match.groups():
                    action = 'name="{0}"'.format(action_name)
                    if action not in current_actions:
                        return [
                            output_api.PresubmitPromptWarning(
                                'File %s line %d: %s is missing in '
                                'tools/metrics/actions/actions.xml. Please run '
                                'tools/metrics/actions/extract_actions.py to update.'
                                % (f.LocalPath(), line_num, action_name))
                        ]
    return []


def _ImportJSONCommentEater(input_api):
    import sys
    sys.path = sys.path + [
        input_api.os_path.join(input_api.PresubmitLocalPath(), 'tools',
                               'json_comment_eater')
    ]
    import json_comment_eater
    return json_comment_eater


def _GetJSONParseError(input_api, filename, eat_comments=True):
    try:
        contents = input_api.ReadFile(filename)
        if eat_comments:
            json_comment_eater = _ImportJSONCommentEater(input_api)
            contents = json_comment_eater.Nom(contents)

        input_api.json.loads(contents)
    except ValueError as e:
        return e
    return None


def _GetIDLParseError(input_api, filename):
    try:
        contents = input_api.ReadFile(filename)
        for i, char in enumerate(contents):
            if not char.isascii():
                return (
                    'Non-ascii character "%s" (ord %d) found at offset %d.' %
                    (char, ord(char), i))
        idl_schema = input_api.os_path.join(input_api.PresubmitLocalPath(),
                                            'tools', 'json_schema_compiler',
                                            'idl_schema.py')
        process = input_api.subprocess.Popen(
            [input_api.python3_executable, idl_schema],
            stdin=input_api.subprocess.PIPE,
            stdout=input_api.subprocess.PIPE,
            stderr=input_api.subprocess.PIPE,
            universal_newlines=True)
        (_, error) = process.communicate(input=contents)
        return error or None
    except ValueError as e:
        return e


def CheckParseErrors(input_api, output_api):
    """Check that IDL and JSON files do not contain syntax errors."""
    actions = {
        '.idl': _GetIDLParseError,
        '.json': _GetJSONParseError,
    }
    # Most JSON files are preprocessed and support comments, but these do not.
    json_no_comments_patterns = [
        r'^testing/',
    ]
    # Only run IDL checker on files in these directories.
    idl_included_patterns = [
        r'^chrome/common/extensions/api/',
        r'^extensions/common/api/',
    ]

    def get_action(affected_file):
        filename = affected_file.LocalPath()
        return actions.get(input_api.os_path.splitext(filename)[1])

    def FilterFile(affected_file):
        action = get_action(affected_file)
        if not action:
            return False
        path = affected_file.LocalPath()

        if _MatchesFile(input_api,
                        _KNOWN_TEST_DATA_AND_INVALID_JSON_FILE_PATTERNS, path):
            return False

        if (action == _GetIDLParseError
                and not _MatchesFile(input_api, idl_included_patterns, path)):
            return False
        return True

    results = []
    for affected_file in input_api.AffectedFiles(file_filter=FilterFile,
                                                 include_deletes=False):
        action = get_action(affected_file)
        kwargs = {}
        if (action == _GetJSONParseError
                and _MatchesFile(input_api, json_no_comments_patterns,
                                 affected_file.LocalPath())):
            kwargs['eat_comments'] = False
        parse_error = action(input_api, affected_file.AbsoluteLocalPath(),
                             **kwargs)
        if parse_error:
            results.append(
                output_api.PresubmitError(
                    '%s could not be parsed: %s' %
                    (affected_file.LocalPath(), parse_error)))
    return results


def CheckJavaStyle(input_api, output_api):
    """Runs checkstyle on changed java files and returns errors if any exist."""

    # Return early if no java files were modified.
    if not any(
            _IsJavaFile(input_api, f.LocalPath())
            for f in input_api.AffectedFiles()):
        return []

    import sys
    original_sys_path = sys.path
    try:
        sys.path = sys.path + [
            input_api.os_path.join(input_api.PresubmitLocalPath(), 'tools',
                                   'android', 'checkstyle')
        ]
        import checkstyle
    finally:
        # Restore sys.path to what it was before.
        sys.path = original_sys_path

    return checkstyle.run_presubmit(
        input_api,
        output_api,
        files_to_skip=_EXCLUDED_PATHS + input_api.DEFAULT_FILES_TO_SKIP)


def CheckPythonDevilInit(input_api, output_api):
    """Checks to make sure devil is initialized correctly in python scripts."""
    script_common_initialize_pattern = input_api.re.compile(
        r'script_common\.InitializeEnvironment\(')
    devil_env_config_initialize = input_api.re.compile(
        r'devil_env\.config\.Initialize\(')

    errors = []

    sources = lambda affected_file: input_api.FilterSourceFile(
        affected_file,
        files_to_skip=(_EXCLUDED_PATHS + input_api.DEFAULT_FILES_TO_SKIP + (
            r'^build/android/devil_chromium\.py',
            r'^tools/bisect-builds\.py',
            r'^third_party/.*',
        )),
        files_to_check=[r'.*\.py$'])

    for f in input_api.AffectedSourceFiles(sources):
        for line_num, line in f.ChangedContents():
            if (script_common_initialize_pattern.search(line)
                    or devil_env_config_initialize.search(line)):
                errors.append("%s:%d" % (f.LocalPath(), line_num))

    results = []

    if errors:
        results.append(
            output_api.PresubmitError(
                'Devil initialization should always be done using '
                'devil_chromium.Initialize() in the chromium project, to use better '
                'defaults for dependencies (ex. up-to-date version of adb).',
                errors))

    return results


def _MatchesFile(input_api, patterns, path):
    # Consistently use / as path separator to simplify the writing of regex
    # expressions.
    path = path.replace(input_api.os_path.sep, '/')
    for pattern in patterns:
        if input_api.re.search(pattern, path):
            return True
    return False


def _ChangeHasSecurityReviewer(input_api, owners_file):
    """Returns True iff the CL has a reviewer from SECURITY_OWNERS.

    Args:
        input_api: The presubmit input API.
        owners_file: OWNERS file with required reviewers. Typically, this is
        something like ipc/SECURITY_OWNERS.

    Note: if the presubmit is running for commit rather than for upload, this
    only returns True if a security reviewer has also approved the CL.
    """
    # Owners-Override should bypass all additional OWNERS enforcement checks.
    # A CR+1 vote will still be required to land this change.
    if (input_api.change.issue and input_api.gerrit.IsOwnersOverrideApproved(
            input_api.change.issue)):
        return True

    owner_email, reviewers = (
        input_api.canned_checks.GetCodereviewOwnerAndReviewers(
            input_api,
            None,
            approval_needed=input_api.is_committing and not input_api.dry_run))

    security_owners = input_api.owners_client.ListOwners(owners_file)
    return any(owner in reviewers for owner in security_owners)


@dataclass
class _SecurityProblemWithItems:
    problem: str
    items: Sequence[str]


@dataclass
class _MissingSecurityOwnersResult:
    owners_file_problems: Sequence[_SecurityProblemWithItems]
    has_security_sensitive_files: bool
    missing_reviewer_problem: Optional[_SecurityProblemWithItems]


def _FindMissingSecurityOwners(input_api,
                               output_api,
                               file_patterns: Sequence[str],
                               excluded_patterns: Sequence[str],
                               required_owners_file: str,
                               custom_rule_function: Optional[Callable] = None
                               ) -> _MissingSecurityOwnersResult:
    """Find OWNERS files missing per-file rules for security-sensitive files.

    Args:
        input_api: the PRESUBMIT input API object.
        output_api: the PRESUBMIT output API object.
        file_patterns: basename patterns that require a corresponding per-file
            security restriction.
        excluded_patterns: path patterns that should be exempted from
            requiring a security restriction.
        required_owners_file: path to the required OWNERS file, e.g.
            ipc/SECURITY_OWNERS
        cc_alias: If not None, email that will be CCed automatically if the
            change contains security-sensitive files, as determined by
            `file_patterns` and `excluded_patterns`.
        custom_rule_function: If not None, will be called with `input_api` and
            the current file under consideration. Returning True will add an
            exact match per-file rule check for the current file.
    """

    # `to_check` is a mapping of an OWNERS file path to Patterns.
    #
    # Patterns is a dictionary mapping glob patterns (suitable for use in
    # per-file rules) to a PatternEntry.
    #
    # PatternEntry is a dictionary with two keys:
    # - 'files': the files that are matched by this pattern
    # - 'rules': the per-file rules needed for this pattern
    #
    # For example, if we expect OWNERS file to contain rules for *.mojom and
    # *_struct_traits*.*, Patterns might look like this:
    # {
    #   '*.mojom': {
    #     'files': ...,
    #     'rules': [
    #       'per-file *.mojom=set noparent',
    #       'per-file *.mojom=file://ipc/SECURITY_OWNERS',
    #     ],
    #   },
    #   '*_struct_traits*.*': {
    #     'files': ...,
    #     'rules': [
    #       'per-file *_struct_traits*.*=set noparent',
    #       'per-file *_struct_traits*.*=file://ipc/SECURITY_OWNERS',
    #     ],
    #   },
    # }
    to_check = {}
    files_to_review = []

    def AddPatternToCheck(file, pattern):
        owners_file = input_api.os_path.join(
            input_api.os_path.dirname(file.LocalPath()), 'OWNERS')
        if owners_file not in to_check:
            to_check[owners_file] = {}
        if pattern not in to_check[owners_file]:
            to_check[owners_file][pattern] = {
                'files': [],
                'rules': [
                    f'per-file {pattern}=set noparent',
                    f'per-file {pattern}=file://{required_owners_file}',
                ]
            }
        to_check[owners_file][pattern]['files'].append(file.LocalPath())
        files_to_review.append(file.LocalPath())

    # Only enforce security OWNERS rules for a directory if that directory has a
    # file that matches `file_patterns`. For example, if a directory only
    # contains *.mojom files and no *_messages*.h files, the check should only
    # ensure that rules for *.mojom files are present.
    for file in input_api.AffectedFiles(include_deletes=False):
        file_basename = input_api.os_path.basename(file.LocalPath())
        if custom_rule_function is not None and custom_rule_function(
                input_api, file):
            AddPatternToCheck(file, file_basename)
            continue

        if any(
                input_api.fnmatch.fnmatch(file.LocalPath(), pattern)
                for pattern in excluded_patterns):
            continue

        for pattern in file_patterns:
            # Unlike `excluded_patterns`, `file_patterns` is checked only against the
            # file's basename.
            if input_api.fnmatch.fnmatch(file_basename, pattern):
                AddPatternToCheck(file, pattern)
                break

    has_security_sensitive_files = bool(to_check)

    # Check if any newly added lines in OWNERS files intersect with required
    # per-file OWNERS lines. If so, ensure that a security reviewer is included.
    # This is a hack, but is needed because the OWNERS check (by design) ignores
    # new OWNERS entries; otherwise, a non-owner could add someone as a new
    # OWNER and have that newly-added OWNER self-approve their own addition.
    newly_covered_files = []
    for file in input_api.AffectedFiles(include_deletes=False):
        if not file.LocalPath() in to_check:
            continue
        for _, line in file.ChangedContents():
            for _, entry in to_check[file.LocalPath()].items():
                if line in entry['rules']:
                    newly_covered_files.extend(entry['files'])

    missing_reviewer_problems = None
    if newly_covered_files and not _ChangeHasSecurityReviewer(
            input_api, required_owners_file):
        missing_reviewer_problems = _SecurityProblemWithItems(
            f'Review from an owner in {required_owners_file} is required for '
            'the following newly-added files:',
            [f'{file}' for file in sorted(set(newly_covered_files))])

    # Go through the OWNERS files to check, filtering out rules that are already
    # present in that OWNERS file.
    for owners_file, patterns in to_check.items():
        try:
            lines = set(
                input_api.ReadFile(
                    input_api.os_path.join(input_api.change.RepositoryRoot(),
                                           owners_file)).splitlines())
            for entry in patterns.values():
                entry['rules'] = [
                    rule for rule in entry['rules'] if rule not in lines
                ]
        except IOError:
            # No OWNERS file, so all the rules are definitely missing.
            continue

    # All the remaining lines weren't found in OWNERS files, so emit an error.
    owners_file_problems = []

    for owners_file, patterns in to_check.items():
        missing_lines = []
        files = []
        for _, entry in patterns.items():
            files.extend(entry['files'])
            missing_lines.extend(entry['rules'])
        if missing_lines:
            joined_missing_lines = '\n'.join(line for line in missing_lines)
            owners_file_problems.append(
                _SecurityProblemWithItems(
                    'Found missing OWNERS lines for security-sensitive files. '
                    f'Please add the following lines to {owners_file}:\n'
                    f'{joined_missing_lines}\n\nTo ensure security review for:',
                    files))

    return _MissingSecurityOwnersResult(owners_file_problems,
                                        has_security_sensitive_files,
                                        missing_reviewer_problems)


def _CheckChangeForIpcSecurityOwners(input_api, output_api):
    # Whether or not a file affects IPC is (mostly) determined by a simple list
    # of filename patterns.
    file_patterns = [
        # Legacy IPC:
        '*_messages.cc',
        '*_messages*.h',
        '*_param_traits*.*',
        # Mojo IPC:
        '*.mojom',
        '*_mojom_traits*.*',
        '*_type_converter*.*',
        # Android native IPC:
        '*.aidl',
    ]

    excluded_patterns = [
        # These third_party directories do not contain IPCs, but contain files
        # matching the above patterns, which trigger false positives.
        'third_party/crashpad/*',
        'third_party/blink/renderer/platform/bindings/*',
        'third_party/protobuf/benchmarks/python/*',
        'third_party/win_build_output/*',
        # Enum-only mojoms used for web metrics, so no security review needed.
        'third_party/blink/public/mojom/use_counter/metrics/*',
        # These files are just used to communicate between class loaders running
        # in the same process.
        'weblayer/browser/java/org/chromium/weblayer_private/interfaces/*',
        'weblayer/browser/java/org/chromium/weblayer_private/test_interfaces/*',
    ]

    def IsMojoServiceManifestFile(input_api, file):
        manifest_pattern = input_api.re.compile('manifests?\.(cc|h)$')
        test_manifest_pattern = input_api.re.compile('test_manifests?\.(cc|h)')
        if not manifest_pattern.search(file.LocalPath()):
            return False

        if test_manifest_pattern.search(file.LocalPath()):
            return False

        # All actual service manifest files should contain at least one
        # qualified reference to service_manager::Manifest.
        return any('service_manager::Manifest' in line
                   for line in file.NewContents())

    return _FindMissingSecurityOwners(
        input_api,
        output_api,
        file_patterns,
        excluded_patterns,
        'ipc/SECURITY_OWNERS',
        custom_rule_function=IsMojoServiceManifestFile)


def _CheckChangeForFuchsiaSecurityOwners(input_api, output_api):
    file_patterns = [
        # Component specifications.
        '*.cml',  # Component Framework v2.
        '*.cmx',  # Component Framework v1.

        # Fuchsia IDL protocol specifications.
        '*.fidl',
    ]

    # Don't check for owners files for changes in these directories.
    excluded_patterns = [
        'third_party/crashpad/*',
    ]

    return _FindMissingSecurityOwners(input_api, output_api, file_patterns,
                                      excluded_patterns,
                                      'build/fuchsia/SECURITY_OWNERS')


def CheckSecurityOwners(input_api, output_api):
    """Checks that various security-sensitive files have an IPC OWNERS rule."""
    ipc_results = _CheckChangeForIpcSecurityOwners(input_api, output_api)
    fuchsia_results = _CheckChangeForFuchsiaSecurityOwners(
        input_api, output_api)

    if ipc_results.has_security_sensitive_files:
        output_api.AppendCC('ipc-security-reviews@chromium.org')

    results = []

    missing_reviewer_problems = []
    if ipc_results.missing_reviewer_problem:
        missing_reviewer_problems.append(ipc_results.missing_reviewer_problem)
    if fuchsia_results.missing_reviewer_problem:
        missing_reviewer_problems.append(
            fuchsia_results.missing_reviewer_problem)

    # Missing reviewers are an error unless there's no issue number
    # associated with this branch; in that case, the presubmit is being run
    # with --all or --files.
    #
    # Note that upload should never be an error; otherwise, it would be
    # impossible to upload changes at all.
    if input_api.is_committing and input_api.change.issue:
        make_presubmit_message = output_api.PresubmitError
    else:
        make_presubmit_message = output_api.PresubmitNotifyResult
    for problem in missing_reviewer_problems:
        results.append(
            make_presubmit_message(problem.problem, items=problem.items))

    owners_file_problems = []
    owners_file_problems.extend(ipc_results.owners_file_problems)
    owners_file_problems.extend(fuchsia_results.owners_file_problems)

    for problem in owners_file_problems:
        # Missing per-file rules are always an error. While swarming and caching
        # means that uploading a patchset with updated OWNERS files and sending
        # it to the CQ again should not have a large incremental cost, it is
        # still frustrating to discover the error only after the change has
        # already been uploaded.
        results.append(
            output_api.PresubmitError(problem.problem, items=problem.items))

    return results


def _GetFilesUsingSecurityCriticalFunctions(input_api):
    """Checks affected files for changes to security-critical calls. This
    function checks the full change diff, to catch both additions/changes
    and removals.

    Returns a dict keyed by file name, and the value is a set of detected
    functions.
    """
    # Map of function pretty name (displayed in an error) to the pattern to
    # match it with.
    _PATTERNS_TO_CHECK = {
        'content::GetServiceSandboxType<>()': 'GetServiceSandboxType\\<'
    }
    _PATTERNS_TO_CHECK = {
        k: input_api.re.compile(v)
        for k, v in _PATTERNS_TO_CHECK.items()
    }

    # We don't want to trigger on strings within this file.
    def presubmit_file_filter(f):
        return 'PRESUBMIT.py' != input_api.os_path.split(f.LocalPath())[1]

    # Scan all affected files for changes touching _FUNCTIONS_TO_CHECK.
    files_to_functions = {}
    for f in input_api.AffectedFiles(file_filter=presubmit_file_filter):
        diff = f.GenerateScmDiff()
        for line in diff.split('\n'):
            # Not using just RightHandSideLines() because removing a
            # call to a security-critical function can be just as important
            # as adding or changing the arguments.
            if line.startswith('-') or (line.startswith('+')
                                        and not line.startswith('++')):
                for name, pattern in _PATTERNS_TO_CHECK.items():
                    if pattern.search(line):
                        path = f.LocalPath()
                        if not path in files_to_functions:
                            files_to_functions[path] = set()
                        files_to_functions[path].add(name)
    return files_to_functions


def CheckSecurityChanges(input_api, output_api):
    """Checks that changes involving security-critical functions are reviewed
    by the security team.
    """
    files_to_functions = _GetFilesUsingSecurityCriticalFunctions(input_api)
    if not len(files_to_functions):
        return []

    owners_file = 'ipc/SECURITY_OWNERS'
    if _ChangeHasSecurityReviewer(input_api, owners_file):
        return []

    msg = 'The following files change calls to security-sensitive functions\n' \
        'that need to be reviewed by {}.\n'.format(owners_file)
    for path, names in files_to_functions.items():
        msg += '  {}\n'.format(path)
        for name in names:
            msg += '    {}\n'.format(name)
        msg += '\n'

    if input_api.is_committing:
        output = output_api.PresubmitError
    else:
        output = output_api.PresubmitNotifyResult
    return [output(msg)]


def CheckSetNoParent(input_api, output_api):
    """Checks that set noparent is only used together with an OWNERS file in
       //build/OWNERS.setnoparent (see also
       //docs/code_reviews.md#owners-files-details)
    """
    # Return early if no OWNERS files were modified.
    if not any(f.LocalPath().endswith('OWNERS')
               for f in input_api.AffectedFiles(include_deletes=False)):
        return []

    errors = []

    allowed_owners_files_file = 'build/OWNERS.setnoparent'
    allowed_owners_files = set()
    with open(allowed_owners_files_file, 'r', encoding='utf-8') as f:
        for line in f:
            line = line.strip()
            if not line or line.startswith('#'):
                continue
            allowed_owners_files.add(line)

    per_file_pattern = input_api.re.compile('per-file (.+)=(.+)')

    for f in input_api.AffectedFiles(include_deletes=False):
        if not f.LocalPath().endswith('OWNERS'):
            continue

        found_owners_files = set()
        found_set_noparent_lines = dict()

        # Parse the OWNERS file.
        for lineno, line in enumerate(f.NewContents(), 1):
            line = line.strip()
            if line.startswith('set noparent'):
                found_set_noparent_lines[''] = lineno
            if line.startswith('file://'):
                if line in allowed_owners_files:
                    found_owners_files.add('')
            if line.startswith('per-file'):
                match = per_file_pattern.match(line)
                if match:
                    glob = match.group(1).strip()
                    directive = match.group(2).strip()
                    if directive == 'set noparent':
                        found_set_noparent_lines[glob] = lineno
                    if directive.startswith('file://'):
                        if directive in allowed_owners_files:
                            found_owners_files.add(glob)

        # Check that every set noparent line has a corresponding file:// line
        # listed in build/OWNERS.setnoparent. An exception is made for top level
        # directories since src/OWNERS shouldn't review them.
        linux_path = f.LocalPath().replace(input_api.os_path.sep, '/')
        if (linux_path.count('/') != 1
                and (not linux_path in _EXCLUDED_SET_NO_PARENT_PATHS)):
            for set_noparent_line in found_set_noparent_lines:
                if set_noparent_line in found_owners_files:
                    continue
                errors.append('  %s:%d' %
                              (linux_path,
                               found_set_noparent_lines[set_noparent_line]))

    results = []
    if errors:
        if input_api.is_committing:
            output = output_api.PresubmitError
        else:
            output = output_api.PresubmitPromptWarning
        results.append(
            output(
                'Found the following "set noparent" restrictions in OWNERS files that '
                'do not include owners from build/OWNERS.setnoparent:',
                long_text='\n\n'.join(errors)))
    return results


def CheckUselessForwardDeclarations(input_api, output_api):
    """Checks that added or removed lines in non third party affected
       header files do not lead to new useless class or struct forward
       declaration.
    """
    results = []
    class_pattern = input_api.re.compile(r'^class\s+(\w+);$',
                                         input_api.re.MULTILINE)
    struct_pattern = input_api.re.compile(r'^struct\s+(\w+);$',
                                          input_api.re.MULTILINE)
    for f in input_api.AffectedFiles(include_deletes=False):
        if (f.LocalPath().startswith('third_party')
                and not f.LocalPath().startswith('third_party/blink')
                and not f.LocalPath().startswith('third_party\\blink')):
            continue

        if not f.LocalPath().endswith('.h'):
            continue

        contents = input_api.ReadFile(f)
        fwd_decls = input_api.re.findall(class_pattern, contents)
        fwd_decls.extend(input_api.re.findall(struct_pattern, contents))

        useless_fwd_decls = []
        for decl in fwd_decls:
            count = sum(1 for _ in input_api.re.finditer(
                r'\b%s\b' % input_api.re.escape(decl), contents))
            if count == 1:
                useless_fwd_decls.append(decl)

        if not useless_fwd_decls:
            continue

        for line in f.GenerateScmDiff().splitlines():
            if (line.startswith('-') and not line.startswith('--')
                    or line.startswith('+') and not line.startswith('++')):
                for decl in useless_fwd_decls:
                    if input_api.re.search(r'\b%s\b' % decl, line[1:]):
                        results.append(
                            output_api.PresubmitPromptWarning(
                                '%s: %s forward declaration is no longer needed'
                                % (f.LocalPath(), decl)))
                        useless_fwd_decls.remove(decl)

    return results


def _CheckAndroidDebuggableBuild(input_api, output_api):
    """Checks that code uses BuildInfo.isDebugAndroid() instead of
       Build.TYPE.equals('') or ''.equals(Build.TYPE) to check if
       this is a debuggable build of Android.
    """
    build_type_check_pattern = input_api.re.compile(
        r'\bBuild\.TYPE\.equals\(|\.equals\(\s*\bBuild\.TYPE\)')

    errors = []

    sources = lambda affected_file: input_api.FilterSourceFile(
        affected_file,
        files_to_skip=(
            _EXCLUDED_PATHS + _TEST_CODE_EXCLUDED_PATHS + input_api.
            DEFAULT_FILES_TO_SKIP + (
                r"^android_webview/support_library/boundary_interfaces/",
                r"^chrome/android/webapk/.*",
                r'^third_party/.*',
                r"tools/android/customtabs_benchmark/.*",
                r"webview/chromium/License.*",
            )),
        files_to_check=[r'.*\.java$'])

    for f in input_api.AffectedSourceFiles(sources):
        for line_num, line in f.ChangedContents():
            if build_type_check_pattern.search(line):
                errors.append("%s:%d" % (f.LocalPath(), line_num))

    results = []

    if errors:
        results.append(
            output_api.PresubmitPromptWarning(
                'Build.TYPE.equals or .equals(Build.TYPE) usage is detected.'
                ' Please use BuildInfo.isDebugAndroid() instead.', errors))

    return results

# TODO: add unit tests
def _CheckAndroidToastUsage(input_api, output_api):
    """Checks that code uses org.chromium.ui.widget.Toast instead of
       android.widget.Toast (Chromium Toast doesn't force hardware
       acceleration on low-end devices, saving memory).
    """
    toast_import_pattern = input_api.re.compile(
        r'^import android\.widget\.Toast;$')

    errors = []

    sources = lambda affected_file: input_api.FilterSourceFile(
        affected_file,
        files_to_skip=(_EXCLUDED_PATHS + _TEST_CODE_EXCLUDED_PATHS + input_api.
                       DEFAULT_FILES_TO_SKIP + (r'^chromecast/.*',
                                                r'^remoting/.*')),
        files_to_check=[r'.*\.java$'])

    for f in input_api.AffectedSourceFiles(sources):
        for line_num, line in f.ChangedContents():
            if toast_import_pattern.search(line):
                errors.append("%s:%d" % (f.LocalPath(), line_num))

    results = []

    if errors:
        results.append(
            output_api.PresubmitError(
                'android.widget.Toast usage is detected. Android toasts use hardware'
                ' acceleration, and can be\ncostly on low-end devices. Please use'
                ' org.chromium.ui.widget.Toast instead.\n'
                'Contact dskiba@chromium.org if you have any questions.',
                errors))

    return results


def _CheckAndroidCrLogUsage(input_api, output_api):
    """Checks that new logs using org.chromium.base.Log:
      - Are using 'TAG' as variable name for the tags (warn)
      - Are using a tag that is shorter than 20 characters (error)
    """

    # Do not check format of logs in the given files
    cr_log_check_excluded_paths = [
        # //chrome/android/webapk cannot depend on //base
        r"^chrome/android/webapk/.*",
        # WebView license viewer code cannot depend on //base; used in stub APK.
        r"^android_webview/glue/java/src/com/android/"
        r"webview/chromium/License.*",
        # The customtabs_benchmark is a small app that does not depend on Chromium
        # java pieces.
        r"tools/android/customtabs_benchmark/.*",
    ]

    cr_log_import_pattern = input_api.re.compile(
        r'^import org\.chromium\.base\.Log;$', input_api.re.MULTILINE)
    class_in_base_pattern = input_api.re.compile(
        r'^package org\.chromium\.base;$', input_api.re.MULTILINE)
    has_some_log_import_pattern = input_api.re.compile(r'^import .*\.Log;$',
                                                       input_api.re.MULTILINE)
    # Extract the tag from lines like `Log.d(TAG, "*");` or `Log.d("TAG", "*");`
    log_call_pattern = input_api.re.compile(r'\bLog\.\w\((?P<tag>\"?\w+)')
    log_decl_pattern = input_api.re.compile(
        r'static final String TAG = "(?P<name>(.*))"')
    rough_log_decl_pattern = input_api.re.compile(r'\bString TAG\s*=')

    REF_MSG = ('See docs/android_logging.md for more info.')
    sources = lambda x: input_api.FilterSourceFile(
        x,
        files_to_check=[r'.*\.java$'],
        files_to_skip=cr_log_check_excluded_paths)

    tag_decl_errors = []
    tag_length_errors = []
    tag_errors = []
    tag_with_dot_errors = []
    util_log_errors = []

    for f in input_api.AffectedSourceFiles(sources):
        file_content = input_api.ReadFile(f)
        has_modified_logs = False
        # Per line checks
        if (cr_log_import_pattern.search(file_content)
                or (class_in_base_pattern.search(file_content)
                    and not has_some_log_import_pattern.search(file_content))):
            # Checks to run for files using cr log
            for line_num, line in f.ChangedContents():
                if rough_log_decl_pattern.search(line):
                    has_modified_logs = True

                # Check if the new line is doing some logging
                match = log_call_pattern.search(line)
                if match:
                    has_modified_logs = True

                    # Make sure it uses "TAG"
                    if not match.group('tag') == 'TAG':
                        tag_errors.append("%s:%d" % (f.LocalPath(), line_num))
        else:
            # Report non cr Log function calls in changed lines
            for line_num, line in f.ChangedContents():
                if log_call_pattern.search(line):
                    util_log_errors.append("%s:%d" % (f.LocalPath(), line_num))

        # Per file checks
        if has_modified_logs:
            # Make sure the tag is using the "cr" prefix and is not too long
            match = log_decl_pattern.search(file_content)
            tag_name = match.group('name') if match else None
            if not tag_name:
                tag_decl_errors.append(f.LocalPath())
            elif len(tag_name) > 20:
                tag_length_errors.append(f.LocalPath())
            elif '.' in tag_name:
                tag_with_dot_errors.append(f.LocalPath())

    results = []
    if tag_decl_errors:
        results.append(
            output_api.PresubmitPromptWarning(
                'Please define your tags using the suggested format: .\n'
                '"private static final String TAG = "<package tag>".\n'
                'They will be prepended with "cr_" automatically.\n' + REF_MSG,
                tag_decl_errors))

    if tag_length_errors:
        results.append(
            output_api.PresubmitError(
                'The tag length is restricted by the system to be at most '
                '20 characters.\n' + REF_MSG, tag_length_errors))

    if tag_errors:
        results.append(
            output_api.PresubmitPromptWarning(
                'Please use a variable named "TAG" for your log tags.\n' +
                REF_MSG, tag_errors))

    if util_log_errors:
        results.append(
            output_api.PresubmitPromptWarning(
                'Please use org.chromium.base.Log for new logs.\n' + REF_MSG,
                util_log_errors))

    if tag_with_dot_errors:
        results.append(
            output_api.PresubmitPromptWarning(
                'Dot in log tags cause them to be elided in crash reports.\n' +
                REF_MSG, tag_with_dot_errors))

    return results


def _CheckAndroidTestAnnotationUsage(input_api, output_api):
    """Checks that android.test.suitebuilder.annotation.* is no longer used."""
    deprecated_annotation_import_pattern = input_api.re.compile(
        r'^import android\.test\.suitebuilder\.annotation\..*;',
        input_api.re.MULTILINE)
    sources = lambda x: input_api.FilterSourceFile(
        x, files_to_check=[r'.*\.java$'], files_to_skip=None)
    errors = []
    for f in input_api.AffectedFiles(file_filter=sources):
        for line_num, line in f.ChangedContents():
            if deprecated_annotation_import_pattern.search(line):
                errors.append("%s:%d" % (f.LocalPath(), line_num))

    results = []
    if errors:
        results.append(
            output_api.PresubmitError(
                'Annotations in android.test.suitebuilder.annotation have been'
                ' deprecated since API level 24. Please use androidx.test.filters'
                ' from //third_party/androidx:androidx_test_runner_java instead.'
                ' Contact yolandyan@chromium.org if you have any questions.',
                errors))
    return results


def _CheckAndroidNewMdpiAssetLocation(input_api, output_api):
    """Checks if MDPI assets are placed in a correct directory."""
    file_filter = lambda f: (f.LocalPath().endswith(
        '.png') and ('/res/drawable/'.replace('/', input_api.os_path.sep) in f.
                     LocalPath() or '/res/drawable-ldrtl/'.replace(
                         '/', input_api.os_path.sep) in f.LocalPath()))
    errors = []
    for f in input_api.AffectedFiles(include_deletes=False,
                                     file_filter=file_filter):
        errors.append('    %s' % f.LocalPath())

    results = []
    if errors:
        results.append(
            output_api.PresubmitError(
                'MDPI assets should be placed in /res/drawable-mdpi/ or '
                '/res/drawable-ldrtl-mdpi/\ninstead of /res/drawable/ and'
                '/res/drawable-ldrtl/.\n'
                'Contact newt@chromium.org if you have questions.', errors))
    return results


def _CheckAndroidWebkitImports(input_api, output_api):
    """Checks that code uses org.chromium.base.Callback instead of
       android.webview.ValueCallback except in the WebView glue layer
       and WebLayer.
    """
    valuecallback_import_pattern = input_api.re.compile(
        r'^import android\.webkit\.ValueCallback;$')

    errors = []

    sources = lambda affected_file: input_api.FilterSourceFile(
        affected_file,
        files_to_skip=(_EXCLUDED_PATHS + _TEST_CODE_EXCLUDED_PATHS + input_api.
                       DEFAULT_FILES_TO_SKIP + (
                           r'^android_webview/glue/.*',
                           r'^android_webview/support_library/.*',
                           r'^weblayer/.*',
                       )),
        files_to_check=[r'.*\.java$'])

    for f in input_api.AffectedSourceFiles(sources):
        for line_num, line in f.ChangedContents():
            if valuecallback_import_pattern.search(line):
                errors.append("%s:%d" % (f.LocalPath(), line_num))

    results = []

    if errors:
        results.append(
            output_api.PresubmitError(
                'android.webkit.ValueCallback usage is detected outside of the glue'
                ' layer. To stay compatible with the support library, android.webkit.*'
                ' classes should only be used inside the glue layer and'
                ' org.chromium.base.Callback should be used instead.', errors))

    return results


def _CheckAndroidXmlStyle(input_api, output_api, is_check_on_upload):
    """Checks Android XML styles """

    # Return early if no relevant files were modified.
    if not any(
            _IsXmlOrGrdFile(input_api, f.LocalPath())
            for f in input_api.AffectedFiles(include_deletes=False)):
        return []

    import sys
    original_sys_path = sys.path
    try:
        sys.path = sys.path + [
            input_api.os_path.join(input_api.PresubmitLocalPath(), 'tools',
                                   'android', 'checkxmlstyle')
        ]
        import checkxmlstyle
    finally:
        # Restore sys.path to what it was before.
        sys.path = original_sys_path

    if is_check_on_upload:
        return checkxmlstyle.CheckStyleOnUpload(input_api, output_api)
    else:
        return checkxmlstyle.CheckStyleOnCommit(input_api, output_api)


def _CheckAndroidInfoBarDeprecation(input_api, output_api):
    """Checks Android Infobar Deprecation """

    import sys
    original_sys_path = sys.path
    try:
        sys.path = sys.path + [
            input_api.os_path.join(input_api.PresubmitLocalPath(), 'tools',
                                   'android', 'infobar_deprecation')
        ]
        import infobar_deprecation
    finally:
        # Restore sys.path to what it was before.
        sys.path = original_sys_path

    return infobar_deprecation.CheckDeprecationOnUpload(input_api, output_api)


class _PydepsCheckerResult:
    def __init__(self, cmd, pydeps_path, process, old_contents):
        self._cmd = cmd
        self._pydeps_path = pydeps_path
        self._process = process
        self._old_contents = old_contents

    def GetError(self):
        """Returns an error message, or None."""
        import difflib
        new_contents = self._process.stdout.read().splitlines()[2:]
        if self._process.wait() != 0:
            # STDERR should already be printed.
            return 'Command failed: ' + self._cmd
        if self._old_contents != new_contents:
            diff = '\n'.join(
                difflib.context_diff(self._old_contents, new_contents))
            return ('File is stale: {}\n'
                    'Diff (apply to fix):\n'
                    '{}\n'
                    'To regenerate, run:\n\n'
                    '    {}').format(self._pydeps_path, diff, self._cmd)
        return None


class PydepsChecker:
    def __init__(self, input_api, pydeps_files):
        self._file_cache = {}
        self._input_api = input_api
        self._pydeps_files = pydeps_files

    def _LoadFile(self, path):
        """Returns the list of paths within a .pydeps file relative to //."""
        if path not in self._file_cache:
            with open(path, encoding='utf-8') as f:
                self._file_cache[path] = f.read()
        return self._file_cache[path]

    def _ComputeNormalizedPydepsEntries(self, pydeps_path):
        """Returns an iterable of paths within the .pydep, relativized to //."""
        pydeps_data = self._LoadFile(pydeps_path)
        uses_gn_paths = '--gn-paths' in pydeps_data
        entries = (l for l in pydeps_data.splitlines()
                   if not l.startswith('#'))
        if uses_gn_paths:
            # Paths look like: //foo/bar/baz
            return (e[2:] for e in entries)
        else:
            # Paths look like: path/relative/to/file.pydeps
            os_path = self._input_api.os_path
            pydeps_dir = os_path.dirname(pydeps_path)
            return (os_path.normpath(os_path.join(pydeps_dir, e))
                    for e in entries)

    def _CreateFilesToPydepsMap(self):
        """Returns a map of local_path -> list_of_pydeps."""
        ret = {}
        for pydep_local_path in self._pydeps_files:
            for path in self._ComputeNormalizedPydepsEntries(pydep_local_path):
                ret.setdefault(path, []).append(pydep_local_path)
        return ret

    def ComputeAffectedPydeps(self):
        """Returns an iterable of .pydeps files that might need regenerating."""
        affected_pydeps = set()
        file_to_pydeps_map = None
        for f in self._input_api.AffectedFiles(include_deletes=True):
            local_path = f.LocalPath()
            # Changes to DEPS can lead to .pydeps changes if any .py files are in
            # subrepositories. We can't figure out which files change, so re-check
            # all files.
            # Changes to print_python_deps.py affect all .pydeps.
            if local_path in ('DEPS', 'PRESUBMIT.py'
                              ) or local_path.endswith('print_python_deps.py'):
                return self._pydeps_files
            elif local_path.endswith('.pydeps'):
                if local_path in self._pydeps_files:
                    affected_pydeps.add(local_path)
            elif local_path.endswith('.py'):
                if file_to_pydeps_map is None:
                    file_to_pydeps_map = self._CreateFilesToPydepsMap()
                affected_pydeps.update(file_to_pydeps_map.get(local_path, ()))
        return affected_pydeps

    def DetermineIfStaleAsync(self, pydeps_path):
        """Runs print_python_deps.py to see if the files is stale."""
        import os

        old_pydeps_data = self._LoadFile(pydeps_path).splitlines()
        if old_pydeps_data:
            cmd = old_pydeps_data[1][1:].strip()
            if '--output' not in cmd:
                cmd += ' --output ' + pydeps_path
            old_contents = old_pydeps_data[2:]
        else:
            # A default cmd that should work in most cases (as long as pydeps filename
            # matches the script name) so that PRESUBMIT.py does not crash if pydeps
            # file is empty/new.
            cmd = 'build/print_python_deps.py {} --root={} --output={}'.format(
                pydeps_path[:-4], os.path.dirname(pydeps_path), pydeps_path)
            old_contents = []
        env = dict(os.environ)
        env['PYTHONDONTWRITEBYTECODE'] = '1'
        process = self._input_api.subprocess.Popen(
            cmd + ' --output ""',
            shell=True,
            env=env,
            stdout=self._input_api.subprocess.PIPE,
            encoding='utf-8')
        return _PydepsCheckerResult(cmd, pydeps_path, process, old_contents)


def _ParseGclientArgs():
    args = {}
    with open('build/config/gclient_args.gni', 'r') as f:
        for line in f:
            line = line.strip()
            if not line or line.startswith('#'):
                continue
            attribute, value = line.split('=')
            args[attribute.strip()] = value.strip()
    return args


def CheckPydepsNeedsUpdating(input_api, output_api, checker_for_tests=None):
    """Checks if a .pydeps file needs to be regenerated."""
    # This check is for Python dependency lists (.pydeps files), and involves
    # paths not only in the PRESUBMIT.py, but also in the .pydeps files. It
    # doesn't work on Windows and Mac, so skip it on other platforms.
    if not input_api.platform.startswith('linux'):
        return []

    results = []
    # First, check for new / deleted .pydeps.
    for f in input_api.AffectedFiles(include_deletes=True):
        # Check whether we are running the presubmit check for a file in src.
        # f.LocalPath is relative to repo (src, or internal repo).
        # os_path.exists is relative to src repo.
        # Therefore if os_path.exists is true, it means f.LocalPath is relative
        # to src and we can conclude that the pydeps is in src.
        if f.LocalPath().endswith('.pydeps'):
            if input_api.os_path.exists(f.LocalPath()):
                if f.Action() == 'D' and f.LocalPath() in _ALL_PYDEPS_FILES:
                    results.append(
                        output_api.PresubmitError(
                            'Please update _ALL_PYDEPS_FILES within //PRESUBMIT.py to '
                            'remove %s' % f.LocalPath()))
                elif f.Action() != 'D' and f.LocalPath(
                ) not in _ALL_PYDEPS_FILES:
                    results.append(
                        output_api.PresubmitError(
                            'Please update _ALL_PYDEPS_FILES within //PRESUBMIT.py to '
                            'include %s' % f.LocalPath()))

    if results:
        return results

    try:
        parsed_args = _ParseGclientArgs()
    except FileNotFoundError:
        message = (
            'build/config/gclient_args.gni not found. Please make sure your '
            'workspace has been initialized with gclient sync.'
        )
        import sys
        original_sys_path = sys.path
        try:
            sys.path = sys.path + [
                input_api.os_path.join(input_api.PresubmitLocalPath(),
                                    'third_party', 'depot_tools')
            ]
            import gclient_utils
            if gclient_utils.IsEnvCog():
                # Users will always hit this when they run presubmits before cog
                # workspace initialization finishes. The check shouldn't fail in
                # this case. This is an unavoidable workaround that's needed for
                # good presubmit UX for cog.
                results.append(output_api.PresubmitPromptWarning(message))
            else:
                results.append(output_api.PresubmitError(message))
            return results
        finally:
            # Restore sys.path to what it was before.
            sys.path = original_sys_path

    is_android = parsed_args.get('checkout_android', 'false') == 'true'
    checker = checker_for_tests or PydepsChecker(input_api, _ALL_PYDEPS_FILES)
    affected_pydeps = set(checker.ComputeAffectedPydeps())
    affected_android_pydeps = affected_pydeps.intersection(
        set(_ANDROID_SPECIFIC_PYDEPS_FILES))
    if affected_android_pydeps and not is_android:
        results.append(
            output_api.PresubmitPromptOrNotify(
                'You have changed python files that may affect pydeps for android\n'
                'specific scripts. However, the relevant presubmit check cannot be\n'
                'run because you are not using an Android checkout. To validate that\n'
                'the .pydeps are correct, re-run presubmit in an Android checkout, or\n'
                'use the android-internal-presubmit optional trybot.\n'
                'Possibly stale pydeps files:\n{}'.format(
                    '\n'.join(affected_android_pydeps))))

    all_pydeps = _ALL_PYDEPS_FILES if is_android else _GENERIC_PYDEPS_FILES
    pydeps_to_check = affected_pydeps.intersection(all_pydeps)
    # Process these concurrently, as each one takes 1-2 seconds.
    pydep_results = [checker.DetermineIfStaleAsync(p) for p in pydeps_to_check]
    for result in pydep_results:
        error_msg = result.GetError()
        if error_msg:
            results.append(output_api.PresubmitError(error_msg))

    return results


def CheckSingletonInHeaders(input_api, output_api):
    """Checks to make sure no header files have |Singleton<|."""

    def FileFilter(affected_file):
        # It's ok for base/memory/singleton.h to have |Singleton<|.
        files_to_skip = (_EXCLUDED_PATHS + input_api.DEFAULT_FILES_TO_SKIP +
                         (r"^base/memory/singleton\.h$",
                          r"^net/quic/platform/impl/quic_singleton_impl\.h$"))
        return input_api.FilterSourceFile(affected_file,
                                          files_to_skip=files_to_skip)

    pattern = input_api.re.compile(r'(?<!class\sbase::)Singleton\s*<')
    files = []
    for f in input_api.AffectedSourceFiles(FileFilter):
        if (f.LocalPath().endswith('.h') or f.LocalPath().endswith('.hxx')
                or f.LocalPath().endswith('.hpp')
                or f.LocalPath().endswith('.inl')):
            contents = input_api.ReadFile(f)
            for line in contents.splitlines(False):
                if (not line.lstrip().startswith('//')
                        and  # Strip C++ comment.
                        pattern.search(line)):
                    files.append(f)
                    break

    if files:
        return [
            output_api.PresubmitError(
                'Found base::Singleton<T> in the following header files.\n' +
                'Please move them to an appropriate source file so that the ' +
                'template gets instantiated in a single compilation unit.',
                files)
        ]
    return []


_DEPRECATED_CSS = [
  # Values
  ( "-webkit-box", "flex" ),
  ( "-webkit-inline-box", "inline-flex" ),
  ( "-webkit-flex", "flex" ),
  ( "-webkit-inline-flex", "inline-flex" ),
  ( "-webkit-min-content", "min-content" ),
  ( "-webkit-max-content", "max-content" ),

  # Properties
  ( "-webkit-background-clip", "background-clip" ),
  ( "-webkit-background-origin", "background-origin" ),
  ( "-webkit-background-size", "background-size" ),
  ( "-webkit-box-shadow", "box-shadow" ),
  ( "-webkit-user-select", "user-select" ),

  # Functions
  ( "-webkit-gradient", "gradient" ),
  ( "-webkit-repeating-gradient", "repeating-gradient" ),
  ( "-webkit-linear-gradient", "linear-gradient" ),
  ( "-webkit-repeating-linear-gradient", "repeating-linear-gradient" ),
  ( "-webkit-radial-gradient", "radial-gradient" ),
  ( "-webkit-repeating-radial-gradient", "repeating-radial-gradient" ),
]


# TODO: add unit tests
def CheckNoDeprecatedCss(input_api, output_api):
    """ Make sure that we don't use deprecated CSS
        properties, functions or values. Our external
        documentation and iOS CSS for dom distiller
        (reader mode) are ignored by the hooks as it
        needs to be consumed by WebKit. """
    results = []
    file_inclusion_pattern = [r".+\.css$"]
    files_to_skip = (_EXCLUDED_PATHS + _TEST_CODE_EXCLUDED_PATHS +
                     input_api.DEFAULT_FILES_TO_SKIP +
                     (r"^chrome/common/extensions/docs", r"^chrome/docs",
                      r"^native_client_sdk",
                      # The NTP team prefers reserving -webkit-line-clamp for
                      # ellipsis effect which can only be used with -webkit-box.
                      r"ui/webui/resources/cr_components/most_visited/.*\.css$"))
    file_filter = lambda f: input_api.FilterSourceFile(
        f, files_to_check=file_inclusion_pattern, files_to_skip=files_to_skip)
    for fpath in input_api.AffectedFiles(file_filter=file_filter):
        for line_num, line in fpath.ChangedContents():
            for (deprecated_value, value) in _DEPRECATED_CSS:
                if deprecated_value in line:
                    results.append(
                        output_api.PresubmitError(
                            "%s:%d: Use of deprecated CSS %s, use %s instead" %
                            (fpath.LocalPath(), line_num, deprecated_value,
                             value)))
    return results


def CheckForRelativeIncludes(input_api, output_api):
    bad_files = {}
    for f in input_api.AffectedFiles(include_deletes=False):
        if (f.LocalPath().startswith('third_party')
                and not f.LocalPath().startswith('third_party/blink')
                and not f.LocalPath().startswith('third_party\\blink')):
            continue

        if not _IsCPlusPlusFile(input_api, f.LocalPath()):
            continue

        relative_includes = [
            line for _, line in f.ChangedContents()
            if "#include" in line and "../" in line
        ]
        if not relative_includes:
            continue
        bad_files[f.LocalPath()] = relative_includes

    if not bad_files:
        return []

    error_descriptions = []
    for file_path, bad_lines in bad_files.items():
        error_description = file_path
        for line in bad_lines:
            error_description += '\n    ' + line
        error_descriptions.append(error_description)

    results = []
    results.append(
        output_api.PresubmitError(
            'You added one or more relative #include paths (including "../").\n'
            'These shouldn\'t be used because they can be used to include headers\n'
            'from code that\'s not correctly specified as a dependency in the\n'
            'relevant BUILD.gn file(s).', error_descriptions))

    return results


def CheckForCcIncludes(input_api, output_api):
    """Check that nobody tries to include a cc file. It's a relatively
    common error which results in duplicate symbols in object
    files. This may not always break the build until someone later gets
    very confusing linking errors."""
    results = []
    for f in input_api.AffectedFiles(include_deletes=False):
        # We let third_party code do whatever it wants
        if (f.LocalPath().startswith('third_party')
                and not f.LocalPath().startswith('third_party/blink')
                and not f.LocalPath().startswith('third_party\\blink')):
            continue

        if not _IsCPlusPlusFile(input_api, f.LocalPath()):
            continue

        for _, line in f.ChangedContents():
            if line.startswith('#include "'):
                included_file = line.split('"')[1]
                if _IsCPlusPlusFile(input_api, included_file):
                    # The most common naming for external files with C++ code,
                    # apart from standard headers, is to call them foo.inc, but
                    # Chromium sometimes uses foo-inc.cc so allow that as well.
                    if not included_file.endswith(('.h', '-inc.cc')):
                        results.append(
                            output_api.PresubmitError(
                                'Only header files or .inc files should be included in other\n'
                                'C++ files. Compiling the contents of a cc file more than once\n'
                                'will cause duplicate information in the build which may later\n'
                                'result in strange link_errors.\n' +
                                f.LocalPath() + ':\n    ' + line))

    return results


def _CheckWatchlistDefinitionsEntrySyntax(key, value, ast):
    if not isinstance(key, ast.Str):
        return 'Key at line %d must be a string literal' % key.lineno
    if not isinstance(value, ast.Dict):
        return 'Value at line %d must be a dict' % value.lineno
    if len(value.keys) != 1:
        return 'Dict at line %d must have single entry' % value.lineno
    if not isinstance(value.keys[0], ast.Str) or value.keys[0].s != 'filepath':
        return (
            'Entry at line %d must have a string literal \'filepath\' as key' %
            value.lineno)
    return None


def _CheckWatchlistsEntrySyntax(key, value, ast, email_regex):
    if not isinstance(key, ast.Str):
        return 'Key at line %d must be a string literal' % key.lineno
    if not isinstance(value, ast.List):
        return 'Value at line %d must be a list' % value.lineno
    for element in value.elts:
        if not isinstance(element, ast.Str):
            return 'Watchlist elements on line %d is not a string' % key.lineno
        if not email_regex.match(element.s):
            return ('Watchlist element on line %d doesn\'t look like a valid '
                    + 'email: %s') % (key.lineno, element.s)
    return None


def _CheckWATCHLISTSEntries(wd_dict, w_dict, input_api):
    mismatch_template = (
        'Mismatch between WATCHLIST_DEFINITIONS entry (%s) and WATCHLISTS '
        'entry (%s)')

    email_regex = input_api.re.compile(
        r"^[A-Za-z0-9._%+-]+@[A-Za-z0-9.-]+\.[A-Za-z]+$")

    ast = input_api.ast
    i = 0
    last_key = ''
    while True:
        if i >= len(wd_dict.keys):
            if i >= len(w_dict.keys):
                return None
            return mismatch_template % ('missing',
                                        'line %d' % w_dict.keys[i].lineno)
        elif i >= len(w_dict.keys):
            return (mismatch_template %
                    ('line %d' % wd_dict.keys[i].lineno, 'missing'))

        wd_key = wd_dict.keys[i]
        w_key = w_dict.keys[i]

        result = _CheckWatchlistDefinitionsEntrySyntax(wd_key,
                                                       wd_dict.values[i], ast)
        if result is not None:
            return 'Bad entry in WATCHLIST_DEFINITIONS dict: %s' % result

        result = _CheckWatchlistsEntrySyntax(w_key, w_dict.values[i], ast,
                                             email_regex)
        if result is not None:
            return 'Bad entry in WATCHLISTS dict: %s' % result

        if wd_key.s != w_key.s:
            return mismatch_template % ('%s at line %d' %
                                        (wd_key.s, wd_key.lineno),
                                        '%s at line %d' %
                                        (w_key.s, w_key.lineno))

        if wd_key.s < last_key:
            return (
                'WATCHLISTS dict is not sorted lexicographically at line %d and %d'
                % (wd_key.lineno, w_key.lineno))
        last_key = wd_key.s

        i = i + 1


def _CheckWATCHLISTSSyntax(expression, input_api):
    ast = input_api.ast
    if not isinstance(expression, ast.Expression):
        return 'WATCHLISTS file must contain a valid expression'
    dictionary = expression.body
    if not isinstance(dictionary, ast.Dict) or len(dictionary.keys) != 2:
        return 'WATCHLISTS file must have single dict with exactly two entries'

    first_key = dictionary.keys[0]
    first_value = dictionary.values[0]
    second_key = dictionary.keys[1]
    second_value = dictionary.values[1]

    if (not isinstance(first_key, ast.Str)
            or first_key.s != 'WATCHLIST_DEFINITIONS'
            or not isinstance(first_value, ast.Dict)):
        return ('The first entry of the dict in WATCHLISTS file must be '
                'WATCHLIST_DEFINITIONS dict')

    if (not isinstance(second_key, ast.Str) or second_key.s != 'WATCHLISTS'
            or not isinstance(second_value, ast.Dict)):
        return ('The second entry of the dict in WATCHLISTS file must be '
                'WATCHLISTS dict')

    return _CheckWATCHLISTSEntries(first_value, second_value, input_api)


def CheckWATCHLISTS(input_api, output_api):
    for f in input_api.AffectedFiles(include_deletes=False):
        if f.LocalPath() == 'WATCHLISTS':
            contents = input_api.ReadFile(f, 'r')

            try:
                # First, make sure that it can be evaluated.
                input_api.ast.literal_eval(contents)
                # Get an AST tree for it and scan the tree for detailed style checking.
                expression = input_api.ast.parse(contents,
                                                 filename='WATCHLISTS',
                                                 mode='eval')
            except ValueError as e:
                return [
                    output_api.PresubmitError('Cannot parse WATCHLISTS file',
                                              long_text=repr(e))
                ]
            except SyntaxError as e:
                return [
                    output_api.PresubmitError('Cannot parse WATCHLISTS file',
                                              long_text=repr(e))
                ]
            except TypeError as e:
                return [
                    output_api.PresubmitError('Cannot parse WATCHLISTS file',
                                              long_text=repr(e))
                ]

            result = _CheckWATCHLISTSSyntax(expression, input_api)
            if result is not None:
                return [output_api.PresubmitError(result)]
            break

    return []

def CheckGnRebasePath(input_api, output_api):
    """Checks that target_gen_dir is not used wtih "//" in rebase_path().

    Developers should use root_build_dir instead of "//" when using target_gen_dir because
    Chromium is sometimes built outside of the source tree.
    """

    def gn_files(f):
        return input_api.FilterSourceFile(f, files_to_check=(r'.+\.gn', ))

    rebase_path_regex = input_api.re.compile(r'rebase_path\(("\$target_gen_dir"|target_gen_dir), ("/"|"//")\)')
    problems = []
    for f in input_api.AffectedSourceFiles(gn_files):
        for line_num, line in f.ChangedContents():
            if rebase_path_regex.search(line):
                problems.append(
                    'Absolute path in rebase_path() in %s:%d' %
                    (f.LocalPath(), line_num))

    if problems:
        return [
            output_api.PresubmitPromptWarning(
                'Using an absolute path in rebase_path()',
                items=sorted(problems),
                long_text=(
                    'rebase_path() should use root_build_dir instead of "/" ',
                    'since builds can be initiated from outside of the source ',
                    'root.'))
        ]
    return []

def CheckGnGlobForward(input_api, output_api):
    """Checks that forward_variables_from(invoker, "*") follows best practices.

    As documented at //build/docs/writing_gn_templates.md
    """

    def gn_files(f):
        return input_api.FilterSourceFile(f, files_to_check=(r'.+\.gni', ))

    problems = []
    for f in input_api.AffectedSourceFiles(gn_files):
        for line_num, line in f.ChangedContents():
            if 'forward_variables_from(invoker, "*")' in line:
                problems.append(
                    'Bare forward_variables_from(invoker, "*") in %s:%d' %
                    (f.LocalPath(), line_num))

    if problems:
        return [
            output_api.PresubmitPromptWarning(
                'forward_variables_from("*") without exclusions',
                items=sorted(problems),
                long_text=(
                    'The variables "visibility" and "test_only" should be '
                    'explicitly listed in forward_variables_from(). For more '
                    'details, see:\n'
                    'https://chromium.googlesource.com/chromium/src/+/HEAD/'
                    'build/docs/writing_gn_templates.md'
                    '#Using-forward_variables_from'))
        ]
    return []

def CheckNewHeaderWithoutGnChangeOnUpload(input_api, output_api):
    """Checks that newly added header files have corresponding GN changes.
    Note that this is only a heuristic. To be precise, run script:
    build/check_gn_headers.py.
    """

    def headers(f):
        return input_api.FilterSourceFile(
            f, files_to_check=(r'.+%s' % _HEADER_EXTENSIONS, ))

    new_headers = []
    for f in input_api.AffectedSourceFiles(headers):
        if f.Action() != 'A':
            continue
        new_headers.append(f.LocalPath())

    def gn_files(f):
        return input_api.FilterSourceFile(f, files_to_check=(r'.+\.gn', ))

    all_gn_changed_contents = ''
    for f in input_api.AffectedSourceFiles(gn_files):
        for _, line in f.ChangedContents():
            all_gn_changed_contents += line

    problems = []
    for header in new_headers:
        basename = input_api.os_path.basename(header)
        if basename not in all_gn_changed_contents:
            problems.append(header)

    if problems:
        return [
            output_api.PresubmitPromptWarning(
                'Missing GN changes for new header files',
                items=sorted(problems),
                long_text=
                'Please double check whether newly added header files need '
                'corresponding changes in gn or gni files.\nThis checking is only a '
                'heuristic. Run build/check_gn_headers.py to be precise.\n'
                'Read https://crbug.com/661774 for more info.')
        ]
    return []


def CheckCorrectProductNameInMessages(input_api, output_api):
    """Check that Chromium-branded strings don't include "Chrome" or vice versa.

    This assumes we won't intentionally reference one product from the other
    product.
    """
    all_problems = []
    test_cases = [{
        "filename_postfix": "google_chrome_strings.grd",
        "correct_name": "Chrome",
        "incorrect_name": "Chromium",
    }, {
        "filename_postfix": "google_chrome_strings.grd",
        "correct_name": "Chrome",
        "incorrect_name": "Chrome for Testing",
    }, {
        "filename_postfix": "chromium_strings.grd",
        "correct_name": "Chromium",
        "incorrect_name": "Chrome",
    }]

    for test_case in test_cases:
        problems = []
        filename_filter = lambda x: x.LocalPath().endswith(test_case[
            "filename_postfix"])

        # Check each new line. Can yield false positives in multiline comments, but
        # easier than trying to parse the XML because messages can have nested
        # children, and associating message elements with affected lines is hard.
        for f in input_api.AffectedSourceFiles(filename_filter):
            for line_num, line in f.ChangedContents():
                if "<message" in line or "<!--" in line or "-->" in line:
                    continue
                if test_case["incorrect_name"] in line:
                    # Chrome for Testing is a special edge case: https://goo.gle/chrome-for-testing#bookmark=id.n1rat320av91
                    if (test_case["correct_name"] == "Chromium" and line.count("Chrome") == line.count("Chrome for Testing")):
                        continue
                    problems.append("Incorrect product name in %s:%d" %
                                    (f.LocalPath(), line_num))

        if problems:
            message = (
                "Strings in %s-branded string files should reference \"%s\", not \"%s\""
                % (test_case["correct_name"], test_case["correct_name"],
                   test_case["incorrect_name"]))
            all_problems.append(
                output_api.PresubmitPromptWarning(message, items=problems))

    return all_problems


def CheckForTooLargeFiles(input_api, output_api):
    """Avoid large files, especially binary files, in the repository since
    git doesn't scale well for those. They will be in everyone's repo
    clones forever, forever making Chromium slower to clone and work
    with."""

    # Uploading files to cloud storage is not trivial so we don't want
    # to set the limit too low, but the upper limit for "normal" large
    # files seems to be 1-2 MB, with a handful around 5-8 MB, so
    # anything over 20 MB is exceptional.
    TOO_LARGE_FILE_SIZE_LIMIT = 20 * 1024 * 1024

    too_large_files = []
    for f in input_api.AffectedFiles():
        # Check both added and modified files (but not deleted files).
        if f.Action() in ('A', 'M'):
            size = input_api.os_path.getsize(f.AbsoluteLocalPath())
            if size > TOO_LARGE_FILE_SIZE_LIMIT:
                too_large_files.append("%s: %d bytes" % (f.LocalPath(), size))

    if too_large_files:
        message = (
            'Do not commit large files to git since git scales badly for those.\n'
            +
            'Instead put the large files in cloud storage and use DEPS to\n' +
            'fetch them.\n' + '\n'.join(too_large_files))
        return [
            output_api.PresubmitError('Too large files found in commit',
                                      long_text=message + '\n')
        ]
    else:
        return []


def CheckFuzzTargetsOnUpload(input_api, output_api):
    """Checks specific for fuzz target sources."""
    EXPORTED_SYMBOLS = [
        'LLVMFuzzerInitialize',
        'LLVMFuzzerCustomMutator',
        'LLVMFuzzerCustomCrossOver',
        'LLVMFuzzerMutate',
    ]

    REQUIRED_HEADER = '#include "testing/libfuzzer/libfuzzer_exports.h"'

    def FilterFile(affected_file):
        """Ignore libFuzzer source code."""
        files_to_check = r'.*fuzz.*\.(h|hpp|hcc|cc|cpp|cxx)$'
        files_to_skip = r"^third_party/libFuzzer"

        return input_api.FilterSourceFile(affected_file,
                                          files_to_check=[files_to_check],
                                          files_to_skip=[files_to_skip])

    files_with_missing_header = []
    for f in input_api.AffectedSourceFiles(FilterFile):
        contents = input_api.ReadFile(f, 'r')
        if REQUIRED_HEADER in contents:
            continue

        if any(symbol in contents for symbol in EXPORTED_SYMBOLS):
            files_with_missing_header.append(f.LocalPath())

    if not files_with_missing_header:
        return []

    long_text = (
        'If you define any of the libFuzzer optional functions (%s), it is '
        'recommended to add \'%s\' directive. Otherwise, the fuzz target may '
        'work incorrectly on Mac (crbug.com/687076).\nNote that '
        'LLVMFuzzerInitialize should not be used, unless your fuzz target needs '
        'to access command line arguments passed to the fuzzer. Instead, prefer '
        'static initialization and shared resources as documented in '
        'https://chromium.googlesource.com/chromium/src/+/main/testing/'
        'libfuzzer/efficient_fuzzing.md#simplifying-initialization_cleanup.\n'
        % (', '.join(EXPORTED_SYMBOLS), REQUIRED_HEADER))

    return [
        output_api.PresubmitPromptWarning(message="Missing '%s' in:" %
                                          REQUIRED_HEADER,
                                          items=files_with_missing_header,
                                          long_text=long_text)
    ]


def _CheckNewImagesWarning(input_api, output_api):
    """
    Warns authors who add images into the repo to make sure their images are
    optimized before committing.
    """
    images_added = False
    image_paths = []
    errors = []
    filter_lambda = lambda x: input_api.FilterSourceFile(
        x,
        files_to_skip=(('(?i).*test', r'.*\/junit\/') + input_api.
                       DEFAULT_FILES_TO_SKIP),
        files_to_check=[r'.*\/(drawable|mipmap)'])
    for f in input_api.AffectedFiles(include_deletes=False,
                                     file_filter=filter_lambda):
        local_path = f.LocalPath().lower()
        if any(
                local_path.endswith(extension)
                for extension in _IMAGE_EXTENSIONS):
            images_added = True
            image_paths.append(f)
    if images_added:
        errors.append(
            output_api.PresubmitPromptWarning(
                'It looks like you are trying to commit some images. If these are '
                'non-test-only images, please make sure to read and apply the tips in '
                'https://chromium.googlesource.com/chromium/src/+/HEAD/docs/speed/'
                'binary_size/optimization_advice.md#optimizing-images\nThis check is '
                'FYI only and will not block your CL on the CQ.', image_paths))
    return errors


def ChecksAndroidSpecificOnUpload(input_api, output_api):
    """Groups upload checks that target android code."""
    results = []
    results.extend(_CheckAndroidCrLogUsage(input_api, output_api))
    results.extend(_CheckAndroidDebuggableBuild(input_api, output_api))
    results.extend(_CheckAndroidNewMdpiAssetLocation(input_api, output_api))
    results.extend(_CheckAndroidToastUsage(input_api, output_api))
    results.extend(_CheckAndroidTestAnnotationUsage(input_api, output_api))
    results.extend(_CheckAndroidWebkitImports(input_api, output_api))
    results.extend(_CheckAndroidXmlStyle(input_api, output_api, True))
    results.extend(_CheckNewImagesWarning(input_api, output_api))
    results.extend(_CheckAndroidNoBannedImports(input_api, output_api))
    results.extend(_CheckAndroidInfoBarDeprecation(input_api, output_api))
    return results


def ChecksAndroidSpecificOnCommit(input_api, output_api):
    """Groups commit checks that target android code."""
    results = []
    results.extend(_CheckAndroidXmlStyle(input_api, output_api, False))
    return results

# TODO(chrishall): could we additionally match on any path owned by
#                  ui/accessibility/OWNERS ?
_ACCESSIBILITY_PATHS = (
    r"^chrome/browser.*/accessibility/",
    r"^chrome/browser/extensions/api/automation.*/",
    r"^chrome/renderer/extensions/accessibility_.*",
    r"^chrome/tests/data/accessibility/",
    r"^content/browser/accessibility/",
    r"^content/renderer/accessibility/",
    r"^content/tests/data/accessibility/",
    r"^extensions/renderer/api/automation/",
    r"^services/accessibility/",
    r"^services/screen_ai/",
    r"^ui/accessibility/",
    r"^ui/views/accessibility/",
)

def CheckAccessibilityRelnotesField(input_api, output_api):
    """Checks that commits to accessibility code contain an AX-Relnotes field in
    their commit message."""

    def FileFilter(affected_file):
        paths = _ACCESSIBILITY_PATHS
        return input_api.FilterSourceFile(affected_file, files_to_check=paths)

    # Only consider changes affecting accessibility paths.
    if not any(input_api.AffectedFiles(file_filter=FileFilter)):
        return []

    # AX-Relnotes can appear in either the description or the footer.
    # When searching the description, require 'AX-Relnotes:' to appear at the
    # beginning of a line.
    ax_regex = input_api.re.compile('ax-relnotes[:=]')
    description_has_relnotes = any(
        ax_regex.match(line)
        for line in input_api.change.DescriptionText().lower().splitlines())

    footer_relnotes = input_api.change.GitFootersFromDescription().get(
        'AX-Relnotes', [])
    if description_has_relnotes or footer_relnotes:
        return []

    # TODO(chrishall): link to Relnotes documentation in message.
    message = (
        "Missing 'AX-Relnotes:' field required for accessibility changes"
        "\n  please add 'AX-Relnotes: [release notes].' to describe any "
        "user-facing changes"
        "\n  otherwise add 'AX-Relnotes: n/a.' if this change has no "
        "user-facing effects"
        "\n  if this is confusing or annoying then please contact members "
        "of ui/accessibility/OWNERS.")

    return [output_api.PresubmitNotifyResult(message)]


_ACCESSIBILITY_EVENTS_TEST_PATH = (
    r"^content/test/data/accessibility/event/.*\.html",
)

_ACCESSIBILITY_TREE_TEST_PATH = (
    r"^content/test/data/accessibility/accname/"
      ".*-expected-(mac|win|uia-win|auralinux).txt",
    r"^content/test/data/accessibility/aria/"
      ".*-expected-(mac|win|uia-win|auralinux).txt",
    r"^content/test/data/accessibility/css/"
      ".*-expected-(mac|win|uia-win|auralinux).txt",
    r"^content/test/data/accessibility/event/"
      ".*-expected-(mac|win|uia-win|auralinux).txt",
    r"^content/test/data/accessibility/html/"
      ".*-expected-(mac|win|uia-win|auralinux).txt",
)

_ACCESSIBILITY_ANDROID_EVENTS_TEST_PATH = (
    r"^.*/WebContentsAccessibilityEventsTest\.java",
)

_ACCESSIBILITY_ANDROID_TREE_TEST_PATH = (
    r"^.*/WebContentsAccessibilityTreeTest\.java",
)

def CheckAccessibilityEventsTestsAreIncludedForAndroid(input_api, output_api):
    """Checks that commits that include a newly added, renamed/moved, or deleted
    test in the DumpAccessibilityEventsTest suite also includes a corresponding
    change to the Android test."""

    def FilePathFilter(affected_file):
        paths = _ACCESSIBILITY_EVENTS_TEST_PATH
        return input_api.FilterSourceFile(affected_file, files_to_check=paths)

    def AndroidFilePathFilter(affected_file):
        paths = _ACCESSIBILITY_ANDROID_EVENTS_TEST_PATH
        return input_api.FilterSourceFile(affected_file, files_to_check=paths)

    # Only consider changes in the events test data path with html type.
    if not any(
            input_api.AffectedFiles(include_deletes=True,
                                    file_filter=FilePathFilter)):
        return []

    # If the commit contains any change to the Android test file, ignore.
    if any(
            input_api.AffectedFiles(include_deletes=True,
                                    file_filter=AndroidFilePathFilter)):
        return []

    # Only consider changes that are adding/renaming or deleting a file
    message = []
    for f in input_api.AffectedFiles(include_deletes=True,
                                     file_filter=FilePathFilter):
        if f.Action() == 'A':
            message = (
                "It appears that you are adding platform expectations for a"
                "\ndump_accessibility_events* test, but have not included"
                "\na corresponding change for Android."
                "\nPlease include the test from:"
                "\n    content/public/android/javatests/src/org/chromium/"
                "content/browser/accessibility/"
                "WebContentsAccessibilityEventsTest.java"
                "\nIf this message is confusing or annoying, please contact"
                "\nmembers of ui/accessibility/OWNERS.")

    # If no message was set, return empty.
    if not len(message):
        return []

    return [output_api.PresubmitPromptWarning(message)]


def CheckAccessibilityTreeTestsAreIncludedForAndroid(input_api, output_api):
    """Checks that commits that include a newly added, renamed/moved, or deleted
    test in the DumpAccessibilityTreeTest suite also includes a corresponding
    change to the Android test."""

    def FilePathFilter(affected_file):
        paths = _ACCESSIBILITY_TREE_TEST_PATH
        return input_api.FilterSourceFile(affected_file, files_to_check=paths)

    def AndroidFilePathFilter(affected_file):
        paths = _ACCESSIBILITY_ANDROID_TREE_TEST_PATH
        return input_api.FilterSourceFile(affected_file, files_to_check=paths)

    # Only consider changes in the various tree test data paths with html type.
    if not any(
            input_api.AffectedFiles(include_deletes=True,
                                    file_filter=FilePathFilter)):
        return []

    # If the commit contains any change to the Android test file, ignore.
    if any(
            input_api.AffectedFiles(include_deletes=True,
                                    file_filter=AndroidFilePathFilter)):
        return []

    # Only consider changes that are adding/renaming or deleting a file
    message = []
    for f in input_api.AffectedFiles(include_deletes=True,
                                     file_filter=FilePathFilter):
        if f.Action() == 'A':
            message = (
                "It appears that you are adding platform expectations for a"
                "\ndump_accessibility_tree* test, but have not included"
                "\na corresponding change for Android."
                "\nPlease include (or remove) the test from:"
                "\n    content/public/android/javatests/src/org/chromium/"
                "content/browser/accessibility/"
                "WebContentsAccessibilityTreeTest.java"
                "\nIf this message is confusing or annoying, please contact"
                "\nmembers of ui/accessibility/OWNERS.")

    # If no message was set, return empty.
    if not len(message):
        return []

    return [output_api.PresubmitPromptWarning(message)]


# string pattern, sequence of strings to show when pattern matches,
# error flag. True if match is a presubmit error, otherwise it's a warning.
_NON_INCLUSIVE_TERMS = (
    (
        # Note that \b pattern in python re is pretty particular. In this
        # regexp, 'class WhiteList ...' will match, but 'class FooWhiteList
        # ...' will not. This may require some tweaking to catch these cases
        # without triggering a lot of false positives. Leaving it naive and
        # less matchy for now.
        r'/(?i)\b((black|white)list|master|slave)\b',  # nocheck
        (
            'Please don\'t use blacklist, whitelist, '  # nocheck
            'or slave in your',  # nocheck
            'code and make every effort to use other terms. Using "// nocheck"',
            '"# nocheck" or "<!-- nocheck -->"',
            'at the end of the offending line will bypass this PRESUBMIT error',
            'but avoid using this whenever possible. Reach out to',
            'community@chromium.org if you have questions'),
        True),)

def ChecksCommon(input_api, output_api):
    """Checks common to both upload and commit."""
    results = []
    results.extend(
        input_api.canned_checks.PanProjectChecks(
            input_api, output_api, excluded_paths=_EXCLUDED_PATHS))

    author = input_api.change.author_email
    if author and author not in _KNOWN_ROBOTS:
        results.extend(
            input_api.canned_checks.CheckAuthorizedAuthor(
                input_api, output_api))

    results.extend(
        input_api.canned_checks.CheckChangeHasNoTabs(
            input_api,
            output_api,
            source_file_filter=lambda x: x.LocalPath().endswith('.grd')))
    results.extend(
        input_api.RunTests(
            input_api.canned_checks.CheckVPythonSpec(input_api, output_api)))

    dirmd = 'dirmd.bat' if input_api.is_windows else 'dirmd'
    dirmd_bin = input_api.os_path.join(input_api.PresubmitLocalPath(),
                                       'third_party', 'depot_tools', dirmd)
    results.extend(
        input_api.RunTests(
            input_api.canned_checks.CheckDirMetadataFormat(
                input_api, output_api, dirmd_bin)))
    results.extend(
        input_api.canned_checks.CheckOwnersDirMetadataExclusive(
            input_api, output_api))
    results.extend(
        input_api.canned_checks.CheckNoNewMetadataInOwners(
            input_api, output_api))
    results.extend(
        input_api.canned_checks.CheckInclusiveLanguage(
            input_api,
            output_api,
            excluded_directories_relative_path=[
                'infra', 'inclusive_language_presubmit_exempt_dirs.txt'
            ],
            non_inclusive_terms=_NON_INCLUSIVE_TERMS))

    presubmit_py_filter = lambda f: input_api.FilterSourceFile(
        f, files_to_check=[r'.*PRESUBMIT\.py$'])
    for f in input_api.AffectedFiles(include_deletes=False,
                                     file_filter=presubmit_py_filter):
        full_path = input_api.os_path.dirname(f.AbsoluteLocalPath())
        test_file = input_api.os_path.join(full_path, 'PRESUBMIT_test.py')
        # The PRESUBMIT.py file (and the directory containing it) might have
        # been affected by being moved or removed, so only try to run the tests
        # if they still exist.
        if not input_api.os_path.exists(test_file):
            continue

        results.extend(
            input_api.canned_checks.RunUnitTestsInDirectory(
                input_api,
                output_api,
                full_path,
                files_to_check=[r'^PRESUBMIT_test\.py$']))
    return results


def CheckPatchFiles(input_api, output_api):
    problems = [
        f.LocalPath() for f in input_api.AffectedFiles()
        if f.LocalPath().endswith(('.orig', '.rej'))
    ]
    # Cargo.toml.orig files are part of third-party crates downloaded from
    # crates.io and should be included.
    problems = [f for f in problems if not f.endswith('Cargo.toml.orig')]
    if problems:
        return [
            output_api.PresubmitError("Don't commit .rej and .orig files.",
                                      problems)
        ]
    else:
        return []


def CheckBuildConfigMacrosWithoutInclude(input_api, output_api):
    # Excludes OS_CHROMEOS, which is not defined in build_config.h.
    macro_re = input_api.re.compile(
        r'^\s*#(el)?if.*\bdefined\(((COMPILER_|ARCH_CPU_|WCHAR_T_IS_)[^)]*)')
    include_re = input_api.re.compile(r'^#include\s+"build/build_config.h"',
                                      input_api.re.MULTILINE)
    extension_re = input_api.re.compile(r'\.[a-z]+$')
    errors = []
    config_h_file = input_api.os_path.join('build', 'build_config.h')
    for f in input_api.AffectedFiles(include_deletes=False):
        # The build-config macros are allowed to be used in build_config.h
        # without including itself.
        if f.LocalPath() == config_h_file:
            continue
        if not f.LocalPath().endswith(
            ('.h', '.c', '.cc', '.cpp', '.m', '.mm')):
            continue

        found_line_number = None
        found_macro = None
        all_lines = input_api.ReadFile(f, 'r').splitlines()
        for line_num, line in enumerate(all_lines):
            match = macro_re.search(line)
            if match:
                found_line_number = line_num
                found_macro = match.group(2)
                break
        if not found_line_number:
            continue

        found_include_line = -1
        for line_num, line in enumerate(all_lines):
            if include_re.search(line):
                found_include_line = line_num
                break
        if found_include_line >= 0 and found_include_line < found_line_number:
            continue

        if not f.LocalPath().endswith('.h'):
            primary_header_path = extension_re.sub('.h', f.AbsoluteLocalPath())
            try:
                content = input_api.ReadFile(primary_header_path, 'r')
                if include_re.search(content):
                    continue
            except IOError:
                pass
        errors.append('%s:%d %s macro is used without first including build/'
                      'build_config.h.' %
                      (f.LocalPath(), found_line_number, found_macro))
    if errors:
        return [output_api.PresubmitPromptWarning('\n'.join(errors))]
    return []


def CheckForSuperfluousStlIncludesInHeaders(input_api, output_api):
    stl_include_re = input_api.re.compile(r'^#include\s+<('
                                          r'algorithm|'
                                          r'array|'
                                          r'limits|'
                                          r'list|'
                                          r'map|'
                                          r'memory|'
                                          r'queue|'
                                          r'set|'
                                          r'string|'
                                          r'unordered_map|'
                                          r'unordered_set|'
                                          r'utility|'
                                          r'vector)>')
    std_namespace_re = input_api.re.compile(r'std::')
    errors = []
    for f in input_api.AffectedFiles():
        if not _IsCPlusPlusHeaderFile(input_api, f.LocalPath()):
            continue

        uses_std_namespace = False
        has_stl_include = False
        for line in f.NewContents():
            if has_stl_include and uses_std_namespace:
                break

            if not has_stl_include and stl_include_re.search(line):
                has_stl_include = True
                continue

            if not uses_std_namespace and (std_namespace_re.search(line)
                    or 'no-std-usage-because-pch-file' in line):
                uses_std_namespace = True
                continue

        if has_stl_include and not uses_std_namespace:
            errors.append(
                '%s: Includes STL header(s) but does not reference std::' %
                f.LocalPath())
    if errors:
        return [output_api.PresubmitPromptWarning('\n'.join(errors))]
    return []


def _CheckForDeprecatedOSMacrosInFile(input_api, f):
    """Check for sensible looking, totally invalid OS macros."""
    preprocessor_statement = input_api.re.compile(r'^\s*#')
    os_macro = input_api.re.compile(r'defined\(OS_([^)]+)\)')
    results = []
    for lnum, line in f.ChangedContents():
        if preprocessor_statement.search(line):
            for match in os_macro.finditer(line):
                results.append(
                    '  %s:%d: %s' %
                    (f.LocalPath(), lnum, 'defined(OS_' + match.group(1) +
                     ') -> BUILDFLAG(IS_' + match.group(1) + ')'))
    return results


def CheckForDeprecatedOSMacros(input_api, output_api):
    """Check all affected files for invalid OS macros."""
    bad_macros = []
    # The OS_ macros are allowed to be used in build/build_config.h.
    config_h_file = input_api.os_path.join('build', 'build_config.h')
    for f in input_api.AffectedSourceFiles(None):
        if not f.LocalPath().endswith(('.py', '.js', '.html', '.css', '.md')) \
                and f.LocalPath() != config_h_file:
            bad_macros.extend(_CheckForDeprecatedOSMacrosInFile(input_api, f))

    if not bad_macros:
        return []

    return [
        output_api.PresubmitError(
            'OS macros have been deprecated. Please use BUILDFLAGs instead (still '
            'defined in build_config.h):', bad_macros)
    ]


def _CheckForInvalidIfDefinedMacrosInFile(input_api, f):
    """Check all affected files for invalid "if defined" macros."""
    ALWAYS_DEFINED_MACROS = (
        "TARGET_CPU_PPC",
        "TARGET_CPU_PPC64",
        "TARGET_CPU_68K",
        "TARGET_CPU_X86",
        "TARGET_CPU_ARM",
        "TARGET_CPU_MIPS",
        "TARGET_CPU_SPARC",
        "TARGET_CPU_ALPHA",
        "TARGET_IPHONE_SIMULATOR",
        "TARGET_OS_EMBEDDED",
        "TARGET_OS_IPHONE",
        "TARGET_OS_MAC",
        "TARGET_OS_UNIX",
        "TARGET_OS_WIN32",
    )
    ifdef_macro = input_api.re.compile(
        r'^\s*#.*(?:ifdef\s|defined\()([^\s\)]+)')
    results = []
    for lnum, line in f.ChangedContents():
        for match in ifdef_macro.finditer(line):
            if match.group(1) in ALWAYS_DEFINED_MACROS:
                always_defined = ' %s is always defined. ' % match.group(1)
                did_you_mean = 'Did you mean \'#if %s\'?' % match.group(1)
                results.append(
                    '    %s:%d %s\n\t%s' %
                    (f.LocalPath(), lnum, always_defined, did_you_mean))
    return results


def CheckForInvalidIfDefinedMacros(input_api, output_api):
    """Check all affected files for invalid "if defined" macros."""
    SKIPPED_PATHS = [
        'base/allocator/partition_allocator/src/partition_alloc/build_config.h',
        'build/build_config.h',
        'third_party/abseil-cpp/',
        'third_party/sqlite/',
    ]
    def affected_files_filter(f):
        # Normalize the local path to Linux-style path separators so that the
        # path comparisons work on Windows as well.
        path = f.LocalPath().replace('\\', '/')

        for skipped_path in SKIPPED_PATHS:
            if path.startswith(skipped_path):
                return False

        return path.endswith(('.h', '.c', '.cc', '.m', '.mm'))

    bad_macros = []
    for f in input_api.AffectedSourceFiles(affected_files_filter):
        bad_macros.extend(_CheckForInvalidIfDefinedMacrosInFile(input_api, f))

    if not bad_macros:
        return []

    return [
        output_api.PresubmitError(
            'Found ifdef check on always-defined macro[s]. Please fix your code\n'
            'or check the list of ALWAYS_DEFINED_MACROS in src/PRESUBMIT.py.',
            bad_macros)
    ]

def CheckForIPCRules(input_api, output_api):
    """Check for same IPC rules described in
    http://www.chromium.org/Home/chromium-security/education/security-tips-for-ipc
    """
    base_pattern = r'IPC_ENUM_TRAITS\('
    inclusion_pattern = input_api.re.compile(r'(%s)' % base_pattern)
    comment_pattern = input_api.re.compile(r'//.*(%s)' % base_pattern)

    problems = []
    for f in input_api.AffectedSourceFiles(None):
        local_path = f.LocalPath()
        if not local_path.endswith('.h'):
            continue
        for line_number, line in f.ChangedContents():
            if inclusion_pattern.search(
                    line) and not comment_pattern.search(line):
                problems.append('%s:%d\n    %s' %
                                (local_path, line_number, line.strip()))

    if problems:
        return [
            output_api.PresubmitPromptWarning(_IPC_ENUM_TRAITS_DEPRECATED,
                                              problems)
        ]
    else:
        return []


def CheckForLongPathnames(input_api, output_api):
    """Check to make sure no files being submitted have long paths.
    This causes issues on Windows.
    """
    problems = []
    for f in input_api.AffectedTestableFiles():
        local_path = f.LocalPath()
        # Windows has a path limit of 260 characters. Limit path length to 200 so
        # that we have some extra for the prefix on dev machines and the bots.
        if (local_path.startswith('third_party/blink/web_tests/platform/') and
            not local_path.startswith('third_party/blink/web_tests/platform/win')):
            # Do not check length of the path for files not used by Windows
            continue
        if len(local_path) > 200:
            problems.append(local_path)

    if problems:
        return [output_api.PresubmitError(_LONG_PATH_ERROR, problems)]
    else:
        return []


def CheckForIncludeGuards(input_api, output_api):
    """Check that header files have proper guards against multiple inclusion.
    If a file should not have such guards (and it probably should) then it
    should include the string "no-include-guard-because-multiply-included" or
    "no-include-guard-because-pch-file".
    """

    def is_chromium_header_file(f):
        # We only check header files under the control of the Chromium
        # project. This excludes:
        # - third_party/*, except blink.
        # - base/allocator/partition_allocator/: PartitionAlloc is a standalone
        #   library used outside of Chrome. Includes are referenced from its
        #   own base directory. It has its own `CheckForIncludeGuards`
        #   PRESUBMIT.py check.
        # - *_message_generator.h: They use include guards in a special,
        #   non-typical way.
        file_with_path = input_api.os_path.normpath(f.LocalPath())
        return (file_with_path.endswith('.h')
                and not file_with_path.endswith('_message_generator.h')
                and not file_with_path.endswith('com_imported_mstscax.h')
                and not file_with_path.startswith('base/allocator/partition_allocator')
                and (not file_with_path.startswith('third_party')
                     or file_with_path.startswith(
                         input_api.os_path.join('third_party', 'blink'))))

    def replace_special_with_underscore(string):
        return input_api.re.sub(r'[+\\/.-]', '_', string)

    errors = []

    for f in input_api.AffectedSourceFiles(is_chromium_header_file):
        guard_name = None
        guard_line_number = None
        seen_guard_end = False
        bypass_checks_at_end_of_file = False

        file_with_path = input_api.os_path.normpath(f.LocalPath())
        base_file_name = input_api.os_path.splitext(
            input_api.os_path.basename(file_with_path))[0]
        upper_base_file_name = base_file_name.upper()

        expected_guard = replace_special_with_underscore(
            file_with_path.upper() + '_')

        # For "path/elem/file_name.h" we should really only accept
        # PATH_ELEM_FILE_NAME_H_ per coding style.  Unfortunately there
        # are too many (1000+) files with slight deviations from the
        # coding style. The most important part is that the include guard
        # is there, and that it's unique, not the name so this check is
        # forgiving for existing files.
        #
        # As code becomes more uniform, this could be made stricter.

        guard_name_pattern_list = [
            # Anything with the right suffix (maybe with an extra _).
            r'\w+_H__?',

            # To cover include guards with old Blink style.
            r'\w+_h',

            # Anything including the uppercase name of the file.
            r'\w*' + input_api.re.escape(
                replace_special_with_underscore(upper_base_file_name)) +
            r'\w*',
        ]
        guard_name_pattern = '|'.join(guard_name_pattern_list)
        guard_pattern = input_api.re.compile(r'#ifndef\s+(' +
                                             guard_name_pattern + ')')

        for line_number, line in enumerate(f.NewContents()):
            if ('no-include-guard-because-multiply-included' in line
                    or 'no-include-guard-because-pch-file' in line):
                bypass_checks_at_end_of_file = True
                break

            if guard_name is None:
                match = guard_pattern.match(line)
                if match:
                    guard_name = match.group(1)
                    guard_line_number = line_number

                    # We allow existing files to use include guards whose names
                    # don't match the chromium style guide, but new files should
                    # get it right.
                    if guard_name != expected_guard:
                        if f.Action() == 'A':  # If file was just 'A'dded
                            errors.append(
                                output_api.PresubmitPromptWarning(
                                    'Header using the wrong include guard name %s'
                                    % guard_name, [
                                        '%s:%d' %
                                        (f.LocalPath(), line_number + 1)
                                    ], 'Expected: %r\nFound:    %r' %
                                    (expected_guard, guard_name)))
            else:
                # The line after #ifndef should have a #define of the same name.
                if line_number == guard_line_number + 1:
                    expected_line = '#define %s' % guard_name
                    if line != expected_line:
                        errors.append(
                            output_api.PresubmitPromptWarning(
                                'Missing "%s" for include guard' %
                                expected_line,
                                ['%s:%d' % (f.LocalPath(), line_number + 1)],
                                'Expected: %r\nGot: %r' %
                                (expected_line, line)))

                if not seen_guard_end and line == '#endif  // %s' % guard_name:
                    seen_guard_end = True
                elif seen_guard_end:
                    if line.strip() != '':
                        errors.append(
                            output_api.PresubmitPromptWarning(
                                'Include guard %s not covering the whole file'
                                % (guard_name), [f.LocalPath()]))
                        break  # Nothing else to check and enough to warn once.

        if bypass_checks_at_end_of_file:
            continue

        if guard_name is None:
            errors.append(
                output_api.PresubmitPromptWarning(
                    'Missing include guard in %s\n'
                    'Recommended name: %s\n'
                    'This check can be disabled by having the string\n'
                    '"no-include-guard-because-multiply-included" or\n'
                    '"no-include-guard-because-pch-file" in the header.'
                    % (f.LocalPath(), expected_guard)))
        elif not seen_guard_end:
            errors.append(
                output_api.PresubmitPromptWarning(
                    'Incorrect or missing include guard #endif in %s\n'
                    'Recommended #endif comment: // %s'
                    % (f.LocalPath(), expected_guard)))

    return errors


def CheckForWindowsLineEndings(input_api, output_api):
    """Check source code and known ascii text files for Windows style line
    endings.
    """
    known_text_files = r'.*\.(txt|html|htm|py|gyp|gypi|gn|isolate|icon)$'

    file_inclusion_pattern = (known_text_files,
                              r'.+%s' % _IMPLEMENTATION_EXTENSIONS,
                              r'.+%s' % _HEADER_EXTENSIONS)

    problems = []
    source_file_filter = lambda f: input_api.FilterSourceFile(
        f, files_to_check=file_inclusion_pattern, files_to_skip=None)
    for f in input_api.AffectedSourceFiles(source_file_filter):
        # Ignore test files that contain crlf intentionally.
        if f.LocalPath().endswith('crlf.txt'):
            continue
        include_file = False
        for line in input_api.ReadFile(f, 'r').splitlines(True):
            if line.endswith('\r\n'):
                include_file = True
        if include_file:
            problems.append(f.LocalPath())

    if problems:
        return [
            output_api.PresubmitPromptWarning(
                'Are you sure that you want '
                'these files to contain Windows style line endings?\n' +
                '\n'.join(problems))
        ]

    return []


def CheckIconFilesForLicenseHeaders(input_api, output_api):
    """Check that .icon files (which are fragments of C++) have license headers.
    """

    icon_files = (r'.*\.icon$', )

    icons = lambda x: input_api.FilterSourceFile(x, files_to_check=icon_files)
    return input_api.canned_checks.CheckLicense(input_api,
                                                output_api,
                                                source_file_filter=icons)


def CheckForUseOfChromeAppsDeprecations(input_api, output_api):
    """Check source code for use of Chrome App technologies being
    deprecated.
    """

    def _CheckForDeprecatedTech(input_api,
                                output_api,
                                detection_list,
                                files_to_check=None,
                                files_to_skip=None):

        if (files_to_check or files_to_skip):
            source_file_filter = lambda f: input_api.FilterSourceFile(
                f, files_to_check=files_to_check, files_to_skip=files_to_skip)
        else:
            source_file_filter = None

        problems = []

        for f in input_api.AffectedSourceFiles(source_file_filter):
            if f.Action() == 'D':
                continue
            for _, line in f.ChangedContents():
                if any(detect in line for detect in detection_list):
                    problems.append(f.LocalPath())

        return problems

    # to avoid this presubmit script triggering warnings
    files_to_skip = ['PRESUBMIT.py', 'PRESUBMIT_test.py']

    problems = []

    # NMF: any files with extensions .nmf or NMF
    _NMF_FILES = r'\.(nmf|NMF)$'
    problems += _CheckForDeprecatedTech(
        input_api,
        output_api,
        detection_list=[''],  # any change to the file will trigger warning
        files_to_check=[r'.+%s' % _NMF_FILES])

    # MANIFEST: any manifest.json that in its diff includes "app":
    _MANIFEST_FILES = r'(manifest\.json)$'
    problems += _CheckForDeprecatedTech(
        input_api,
        output_api,
        detection_list=['"app":'],
        files_to_check=[r'.*%s' % _MANIFEST_FILES])

    # NaCl / PNaCl: any file that in its diff contains the strings in the list
    problems += _CheckForDeprecatedTech(
        input_api,
        output_api,
        detection_list=['config=nacl', 'enable-nacl', 'cpu=pnacl', 'nacl_io'],
        files_to_skip=files_to_skip + [r"^native_client_sdk/"])

    # PPAPI: any C/C++ file that in its diff includes a ppapi library
    problems += _CheckForDeprecatedTech(
        input_api,
        output_api,
        detection_list=['#include "ppapi', '#include <ppapi'],
        files_to_check=(r'.+%s' % _HEADER_EXTENSIONS,
                        r'.+%s' % _IMPLEMENTATION_EXTENSIONS),
        files_to_skip=[r"^ppapi/"])

    if problems:
        return [
            output_api.PresubmitPromptWarning(
                'You are adding/modifying code'
                'related to technologies which will soon be deprecated (Chrome Apps, NaCl,'
                ' PNaCl, PPAPI). See this blog post for more details:\n'
                'https://blog.chromium.org/2020/08/changes-to-chrome-app-support-timeline.html\n'
                'and this documentation for options to replace these technologies:\n'
                'https://developer.chrome.com/docs/apps/migration/\n' +
                '\n'.join(problems))
        ]

    return []


def CheckSyslogUseWarningOnUpload(input_api, output_api, src_file_filter=None):
    """Checks that all source files use SYSLOG properly."""
    syslog_files = []
    for f in input_api.AffectedSourceFiles(src_file_filter):
        for line_number, line in f.ChangedContents():
            if 'SYSLOG' in line:
                syslog_files.append(f.LocalPath() + ':' + str(line_number))

    if syslog_files:
        return [
            output_api.PresubmitPromptWarning(
                'Please make sure there are no privacy sensitive bits of data in SYSLOG'
                ' calls.\nFiles to check:\n',
                items=syslog_files)
        ]
    return []


def CheckChangeOnUpload(input_api, output_api):
    if input_api.version < [2, 0, 0]:
        return [
            output_api.PresubmitError(
                "Your depot_tools is out of date. "
                "This PRESUBMIT.py requires at least presubmit_support version 2.0.0, "
                "but your version is %d.%d.%d" % tuple(input_api.version))
        ]
    results = []
    results.extend(
        input_api.canned_checks.CheckPatchFormatted(input_api, output_api))
    return results


def CheckChangeOnCommit(input_api, output_api):
    if input_api.version < [2, 0, 0]:
        return [
            output_api.PresubmitError(
                "Your depot_tools is out of date. "
                "This PRESUBMIT.py requires at least presubmit_support version 2.0.0, "
                "but your version is %d.%d.%d" % tuple(input_api.version))
        ]

    results = []
    # Make sure the tree is 'open'.
    results.extend(
        input_api.canned_checks.CheckTreeIsOpen(
            input_api,
            output_api,
            json_url='http://chromium-status.appspot.com/current?format=json'))

    results.extend(
        input_api.canned_checks.CheckPatchFormatted(input_api, output_api))
    results.extend(
        input_api.canned_checks.CheckChangeHasBugField(input_api, output_api))
    results.extend(
        input_api.canned_checks.CheckChangeHasNoUnwantedTags(
            input_api, output_api))
    return results


def CheckStrings(input_api, output_api):
    """Check string ICU syntax validity and if translation screenshots exist."""
    # Skip translation screenshots check if a SkipTranslationScreenshotsCheck
    # footer is set to true.
    git_footers = input_api.change.GitFootersFromDescription()
    skip_screenshot_check_footer = [
        footer.lower() for footer in git_footers.get(
            u'Skip-Translation-Screenshots-Check', [])
    ]
    run_screenshot_check = u'true' not in skip_screenshot_check_footer

    import os
    import re
    import sys
    from io import StringIO

    new_or_added_paths = set(f.LocalPath() for f in input_api.AffectedFiles()
                             if (f.Action() == 'A' or f.Action() == 'M'))
    removed_paths = set(f.LocalPath()
                        for f in input_api.AffectedFiles(include_deletes=True)
                        if f.Action() == 'D')

    affected_grds = [
        f for f in input_api.AffectedFiles()
        if f.LocalPath().endswith(('.grd', '.grdp'))
    ]
    affected_grds = [
        f for f in affected_grds if not 'testdata' in f.LocalPath()
    ]
    if not affected_grds:
        return []

    affected_png_paths = [
        f.AbsoluteLocalPath() for f in input_api.AffectedFiles()
        if (f.LocalPath().endswith('.png'))
    ]

    # Check for screenshots. Developers can upload screenshots using
    # tools/translation/upload_screenshots.py which finds and uploads
    # images associated with .grd files (e.g. test_grd/IDS_STRING.png for the
    # message named IDS_STRING in test.grd) and produces a .sha1 file (e.g.
    # test_grd/IDS_STRING.png.sha1) for each png when the upload is successful.
    #
    # The logic here is as follows:
    #
    # - If the CL has a .png file under the screenshots directory for a grd
    #   file, warn the developer. Actual images should never be checked into the
    #   Chrome repo.
    #
    # - If the CL contains modified or new messages in grd files and doesn't
    #   contain the corresponding .sha1 files, warn the developer to add images
    #   and upload them via tools/translation/upload_screenshots.py.
    #
    # - If the CL contains modified or new messages in grd files and the
    #   corresponding .sha1 files, everything looks good.
    #
    # - If the CL contains removed messages in grd files but the corresponding
    #   .sha1 files aren't removed, warn the developer to remove them.
    unnecessary_screenshots = []
    invalid_sha1 = []
    missing_sha1 = []
    missing_sha1_modified = []
    unnecessary_sha1_files = []

    # This checks verifies that the ICU syntax of messages this CL touched is
    # valid, and reports any found syntax errors.
    # Without this presubmit check, ICU syntax errors in Chromium strings can land
    # without developers being aware of them. Later on, such ICU syntax errors
    # break message extraction for translation, hence would block Chromium
    # translations until they are fixed.
    icu_syntax_errors = []
    sha1_pattern = input_api.re.compile(r'^[a-fA-F0-9]{40}$',
                                        input_api.re.MULTILINE)

    def _CheckScreenshotAdded(screenshots_dir, message_id):
        sha1_path = input_api.os_path.join(screenshots_dir,
                                           message_id + '.png.sha1')
        if sha1_path not in new_or_added_paths:
            missing_sha1.append(sha1_path)
        elif not _CheckValidSha1(sha1_path):
            invalid_sha1.append(sha1_path)

    def _CheckScreenshotModified(screenshots_dir, message_id):
        sha1_path = input_api.os_path.join(screenshots_dir,
                                           message_id + '.png.sha1')
        if sha1_path not in new_or_added_paths:
            missing_sha1_modified.append(sha1_path)
        elif not _CheckValidSha1(sha1_path):
            invalid_sha1.append(sha1_path)

    def _CheckValidSha1(sha1_path):
        return sha1_pattern.search(
            next("\n".join(f.NewContents()) for f in input_api.AffectedFiles()
                 if f.LocalPath() == sha1_path))

    def _CheckScreenshotRemoved(screenshots_dir, message_id):
        sha1_path = input_api.os_path.join(screenshots_dir,
                                           message_id + '.png.sha1')
        if input_api.os_path.exists(
                sha1_path) and sha1_path not in removed_paths:
            unnecessary_sha1_files.append(sha1_path)

    def _ValidateIcuSyntax(text, level, signatures):
        """Validates ICU syntax of a text string.

        Check if text looks similar to ICU and checks for ICU syntax correctness
        in this case. Reports various issues with ICU syntax and values of
        variants. Supports checking of nested messages. Accumulate information of
        each ICU messages found in the text for further checking.

        Args:
          text: a string to check.
          level: a number of current nesting level.
          signatures: an accumulator, a list of tuple of (level, variable,
            kind, variants).

        Returns:
          None if a string is not ICU or no issue detected.
          A tuple of (message, start index, end index) if an issue detected.
        """
        valid_types = {
            'plural': (frozenset(
                ['=0', '=1', '=2', '=3', 'zero', 'one', 'two', 'few', 'many',
                 'other']), frozenset(['=1', 'other'])),
            'selectordinal': (frozenset(
                ['=0', '=1', '=2', '=3', 'zero', 'one', 'two', 'few', 'many',
                 'other']), frozenset(['one', 'other'])),
            'select': (frozenset(), frozenset(['other'])),
        }

        # Check if the message looks like an attempt to use ICU
        # plural. If yes - check if its syntax strictly matches ICU format.
        like = re.match(r'^[^{]*\{[^{]*\b(plural|selectordinal|select)\b',
                        text)
        if not like:
            signatures.append((level, None, None, None))
            return

        # Check for valid prefix and suffix
        m = re.match(
            r'^([^{]*\{)([a-zA-Z0-9_]+),\s*'
            r'(plural|selectordinal|select),\s*'
            r'(?:offset:\d+)?\s*(.*)', text, re.DOTALL)
        if not m:
            return (('This message looks like an ICU plural, '
                     'but does not follow ICU syntax.'), like.start(),
                    like.end())
        starting, variable, kind, variant_pairs = m.groups()
        variants, depth, last_pos = _ParseIcuVariants(variant_pairs,
                                                      m.start(4))
        if depth:
            return ('Invalid ICU format. Unbalanced opening bracket', last_pos,
                    len(text))
        first = text[0]
        ending = text[last_pos:]
        if not starting:
            return ('Invalid ICU format. No initial opening bracket',
                    last_pos - 1, last_pos)
        if not ending or '}' not in ending:
            return ('Invalid ICU format. No final closing bracket',
                    last_pos - 1, last_pos)
        elif first != '{':
            return ((
                'Invalid ICU format. Extra characters at the start of a complex '
                'message (go/icu-message-migration): "%s"') % starting, 0,
                    len(starting))
        elif ending != '}':
            return ((
                'Invalid ICU format. Extra characters at the end of a complex '
                'message (go/icu-message-migration): "%s"') % ending,
                    last_pos - 1, len(text) - 1)
        if kind not in valid_types:
            return (('Unknown ICU message type %s. '
                     'Valid types are: plural, select, selectordinal') % kind,
                    0, 0)
        known, required = valid_types[kind]
        defined_variants = set()
        for variant, variant_range, value, value_range in variants:
            start, end = variant_range
            if variant in defined_variants:
                return ('Variant "%s" is defined more than once' % variant,
                        start, end)
            elif known and variant not in known:
                return ('Variant "%s" is not valid for %s message' %
                        (variant, kind), start, end)
            defined_variants.add(variant)
            # Check for nested structure
            res = _ValidateIcuSyntax(value[1:-1], level + 1, signatures)
            if res:
                return (res[0], res[1] + value_range[0] + 1,
                        res[2] + value_range[0] + 1)
        missing = required - defined_variants
        if missing:
            return ('Required variants missing: %s' % ', '.join(missing), 0,
                    len(text))
        signatures.append((level, variable, kind, defined_variants))

    def _ParseIcuVariants(text, offset=0):
        """Parse variants part of ICU complex message.

        Builds a tuple of variant names and values, as well as
        their offsets in the input string.

        Args:
          text: a string to parse
          offset: additional offset to add to positions in the text to get correct
            position in the complete ICU string.

        Returns:
          List of tuples, each tuple consist of four fields: variant name,
          variant name span (tuple of two integers), variant value, value
          span (tuple of two integers).
        """
        depth, start, end = 0, -1, -1
        variants = []
        key = None
        for idx, char in enumerate(text):
            if char == '{':
                if not depth:
                    start = idx
                    chunk = text[end + 1:start]
                    key = chunk.strip()
                    pos = offset + end + 1 + chunk.find(key)
                    span = (pos, pos + len(key))
                depth += 1
            elif char == '}':
                if not depth:
                    return variants, depth, offset + idx
                depth -= 1
                if not depth:
                    end = idx
                    variants.append((key, span, text[start:end + 1],
                                     (offset + start, offset + end + 1)))
        return variants, depth, offset + end + 1

    try:
        old_sys_path = sys.path
        sys.path = sys.path + [
            input_api.os_path.join(input_api.PresubmitLocalPath(), 'tools',
                                   'translation')
        ]
        from helper import grd_helper
    finally:
        sys.path = old_sys_path

    for f in affected_grds:
        file_path = f.LocalPath()
        old_id_to_msg_map = {}
        new_id_to_msg_map = {}
        # Note that this code doesn't check if the file has been deleted. This is
        # OK because it only uses the old and new file contents and doesn't load
        # the file via its path.
        # It's also possible that a file's content refers to a renamed or deleted
        # file via a <part> tag, such as <part file="now-deleted-file.grdp">. This
        # is OK as well, because grd_helper ignores <part> tags when loading .grd or
        # .grdp files.
        if file_path.endswith('.grdp'):
            if f.OldContents():
                old_id_to_msg_map = grd_helper.GetGrdpMessagesFromString(
                    '\n'.join(f.OldContents()))
            if f.NewContents():
                new_id_to_msg_map = grd_helper.GetGrdpMessagesFromString(
                    '\n'.join(f.NewContents()))
        else:
            file_dir = input_api.os_path.dirname(file_path) or '.'
            if f.OldContents():
                old_id_to_msg_map = grd_helper.GetGrdMessages(
                    StringIO('\n'.join(f.OldContents())), file_dir)
            if f.NewContents():
                new_id_to_msg_map = grd_helper.GetGrdMessages(
                    StringIO('\n'.join(f.NewContents())), file_dir)

        grd_name, ext = input_api.os_path.splitext(
            input_api.os_path.basename(file_path))
        screenshots_dir = input_api.os_path.join(
            input_api.os_path.dirname(file_path),
            grd_name + ext.replace('.', '_'))

        # Compute added, removed and modified message IDs.
        old_ids = set(old_id_to_msg_map)
        new_ids = set(new_id_to_msg_map)
        added_ids = new_ids - old_ids
        removed_ids = old_ids - new_ids
        modified_ids = set([])
        for key in old_ids.intersection(new_ids):
            if (old_id_to_msg_map[key].ContentsAsXml('', True) !=
                    new_id_to_msg_map[key].ContentsAsXml('', True)):
                # The message content itself changed. Require an updated screenshot.
                modified_ids.add(key)
            elif old_id_to_msg_map[key].attrs['meaning'] != \
                new_id_to_msg_map[key].attrs['meaning']:
                # The message meaning changed. We later check for a screenshot.
                modified_ids.add(key)

        if run_screenshot_check:
            # Check the screenshot directory for .png files. Warn if there is any.
            for png_path in affected_png_paths:
                if png_path.startswith(screenshots_dir):
                    unnecessary_screenshots.append(png_path)

            for added_id in added_ids:
                _CheckScreenshotAdded(screenshots_dir, added_id)

            for modified_id in modified_ids:
                _CheckScreenshotModified(screenshots_dir, modified_id)

            for removed_id in removed_ids:
                _CheckScreenshotRemoved(screenshots_dir, removed_id)

        # Check new and changed strings for ICU syntax errors.
        for key in added_ids.union(modified_ids):
            msg = new_id_to_msg_map[key].ContentsAsXml('', True)
            err = _ValidateIcuSyntax(msg, 0, [])
            if err is not None:
                icu_syntax_errors.append(str(key) + ': ' + str(err[0]))

    results = []
    if run_screenshot_check:
        if unnecessary_screenshots:
            results.append(
                output_api.PresubmitError(
                    'Do not include actual screenshots in the changelist. Run '
                    'tools/translate/upload_screenshots.py to upload them instead:',
                    sorted(unnecessary_screenshots)))

        if missing_sha1:
            results.append(
                output_api.PresubmitError(
                    'You are adding UI strings.\n'
                    'To ensure the best translations, take screenshots of the relevant UI '
                    '(https://g.co/chrome/translation) and add these files to your '
                    'changelist:', sorted(missing_sha1)))

        if invalid_sha1:
            results.append(
                output_api.PresubmitError(
                    'The following files do not seem to contain valid sha1 hashes. '
                    'Make sure they contain hashes created by '
                    'tools/translate/upload_screenshots.py:', sorted(invalid_sha1)))

        if missing_sha1_modified:
            results.append(
                output_api.PresubmitError(
                    'You are modifying UI strings or their meanings.\n'
                    'To ensure the best translations, take screenshots of the relevant UI '
                    '(https://g.co/chrome/translation) and add these files to your '
                    'changelist:', sorted(missing_sha1_modified)))

        if unnecessary_sha1_files:
            results.append(
                output_api.PresubmitError(
                    'You removed strings associated with these files. Remove:',
                    sorted(unnecessary_sha1_files)))
    else:
        results.append(
            output_api.PresubmitPromptOrNotify('Skipping translation '
                                               'screenshots check.'))

    if icu_syntax_errors:
        results.append(
            output_api.PresubmitPromptWarning(
                'ICU syntax errors were found in the following strings (problems or '
                'feedback? Contact rainhard@chromium.org):',
                items=icu_syntax_errors))

    return results


def CheckTranslationExpectations(input_api, output_api,
                                  repo_root=None,
                                  translation_expectations_path=None,
                                  grd_files=None):
    import sys
    affected_grds = [
        f for f in input_api.AffectedFiles()
        if (f.LocalPath().endswith('.grd') or f.LocalPath().endswith('.grdp'))
    ]
    if not affected_grds:
        return []

    try:
        old_sys_path = sys.path
        sys.path = sys.path + [
            input_api.os_path.join(input_api.PresubmitLocalPath(), 'tools',
                                   'translation')
        ]
        from helper import git_helper
        from helper import translation_helper
    finally:
        sys.path = old_sys_path

    # Check that translation expectations can be parsed and we can get a list of
    # translatable grd files. |repo_root| and |translation_expectations_path| are
    # only passed by tests.
    if not repo_root:
        repo_root = input_api.PresubmitLocalPath()
    if not translation_expectations_path:
        translation_expectations_path = input_api.os_path.join(
            repo_root, 'tools', 'gritsettings', 'translation_expectations.pyl')
    if not grd_files:
        grd_files = git_helper.list_grds_in_repository(repo_root)

    # Ignore bogus grd files used only for testing
    # ui/webui/resources/tools/generate_grd.py.
    ignore_path = input_api.os_path.join('ui', 'webui', 'resources', 'tools',
                                         'tests')
    grd_files = [p for p in grd_files if ignore_path not in p]

    try:
        translation_helper.get_translatable_grds(
            repo_root, grd_files, translation_expectations_path)
    except Exception as e:
        return [
            output_api.PresubmitNotifyResult(
                'Failed to get a list of translatable grd files. This happens when:\n'
                ' - One of the modified grd or grdp files cannot be parsed or\n'
                ' - %s is not updated.\n'
                'Stack:\n%s' % (translation_expectations_path, str(e)))
        ]
    return []


def CheckStableMojomChanges(input_api, output_api):
    """Changes to [Stable] mojom types must preserve backward-compatibility."""
    changed_mojoms = input_api.AffectedFiles(
        include_deletes=True,
        file_filter=lambda f: f.LocalPath().endswith(('.mojom')))

    if not changed_mojoms or input_api.no_diffs:
        return []

    delta = []
    for mojom in changed_mojoms:
        delta.append({
            'filename': mojom.LocalPath(),
            'old': '\n'.join(mojom.OldContents()) or None,
            'new': '\n'.join(mojom.NewContents()) or None,
        })

    process = input_api.subprocess.Popen([
        input_api.python3_executable,
        input_api.os_path.join(
            input_api.PresubmitLocalPath(), 'mojo', 'public', 'tools', 'mojom',
            'check_stable_mojom_compatibility.py'), '--src-root',
        input_api.PresubmitLocalPath()
    ],
                                         stdin=input_api.subprocess.PIPE,
                                         stdout=input_api.subprocess.PIPE,
                                         stderr=input_api.subprocess.PIPE,
                                         universal_newlines=True)
    (x, error) = process.communicate(input=input_api.json.dumps(delta))
    if process.returncode:
        return [
            output_api.PresubmitError(
                'One or more [Stable] mojom definitions appears to have been changed '
                'in a way that is not backward-compatible. See '
                'https://chromium.googlesource.com/chromium/src/+/HEAD/mojo/public/tools/bindings/README.md#versioning'
                ' for details.',
                long_text=error)
        ]
    return []

def CheckDeprecationOfPreferences(input_api, output_api):
    """Removing a preference should come with a deprecation."""

    def FilterFile(affected_file):
        """Accept only .cc files and the like."""
        file_inclusion_pattern = [r'.+%s' % _IMPLEMENTATION_EXTENSIONS]
        files_to_skip = (_EXCLUDED_PATHS + _TEST_CODE_EXCLUDED_PATHS +
                         input_api.DEFAULT_FILES_TO_SKIP)
        return input_api.FilterSourceFile(
            affected_file,
            files_to_check=file_inclusion_pattern,
            files_to_skip=files_to_skip)

    def ModifiedLines(affected_file):
        """Returns a list of tuples (line number, line text) of added and removed
        lines.

        Deleted lines share the same line number as the previous line.

        This relies on the scm diff output describing each changed code section
        with a line of the form

        ^@@ <old line num>,<old size> <new line num>,<new size> @@$
        """
        line_num = 0
        modified_lines = []
        for line in affected_file.GenerateScmDiff().splitlines():
            # Extract <new line num> of the patch fragment (see format above).
            m = input_api.re.match(r'^@@ [0-9\,\+\-]+ \+([0-9]+)\,[0-9]+ @@',
                                   line)
            if m:
                line_num = int(m.groups(1)[0])
                continue
            if ((line.startswith('+') and not line.startswith('++'))
                    or (line.startswith('-') and not line.startswith('--'))):
                modified_lines.append((line_num, line))

            if not line.startswith('-'):
                line_num += 1
        return modified_lines

    def FindLineWith(lines, needle):
        """Returns the line number (i.e. index + 1) in `lines` containing `needle`.

        If 0 or >1 lines contain `needle`, -1 is returned.
        """
        matching_line_numbers = [
            # + 1 for 1-based counting of line numbers.
            i + 1 for i, line in enumerate(lines) if needle in line
        ]
        return matching_line_numbers[0] if len(
            matching_line_numbers) == 1 else -1

    def ModifiedPrefMigration(affected_file):
        """Returns whether the MigrateObsolete.*Pref functions were modified."""
        # Determine first and last lines of MigrateObsolete.*Pref functions.
        new_contents = affected_file.NewContents()
        range_1 = (FindLineWith(new_contents,
                                'BEGIN_MIGRATE_OBSOLETE_LOCAL_STATE_PREFS'),
                   FindLineWith(new_contents,
                                'END_MIGRATE_OBSOLETE_LOCAL_STATE_PREFS'))
        range_2 = (FindLineWith(new_contents,
                                'BEGIN_MIGRATE_OBSOLETE_PROFILE_PREFS'),
                   FindLineWith(new_contents,
                                'END_MIGRATE_OBSOLETE_PROFILE_PREFS'))
        if (-1 in range_1 + range_2):
            raise Exception(
                'Broken .*MIGRATE_OBSOLETE_.*_PREFS markers in browser_prefs.cc.'
            )

        # Check whether any of the modified lines are part of the
        # MigrateObsolete.*Pref functions.
        for line_nr, line in ModifiedLines(affected_file):
            if (range_1[0] <= line_nr <= range_1[1]
                    or range_2[0] <= line_nr <= range_2[1]):
                return True
        return False

    register_pref_pattern = input_api.re.compile(r'Register.+Pref')
    browser_prefs_file_pattern = input_api.re.compile(
        r'chrome/browser/prefs/browser_prefs.cc')

    changes = input_api.AffectedFiles(include_deletes=True,
                                      file_filter=FilterFile)
    potential_problems = []
    for f in changes:
        for line in f.GenerateScmDiff().splitlines():
            # Check deleted lines for pref registrations.
            if (line.startswith('-') and not line.startswith('--')
                    and register_pref_pattern.search(line)):
                potential_problems.append('%s: %s' % (f.LocalPath(), line))

        if browser_prefs_file_pattern.search(f.LocalPath()):
            # If the developer modified the MigrateObsolete.*Prefs() functions, we
            # assume that they knew that they have to deprecate preferences and don't
            # warn.
            try:
                if ModifiedPrefMigration(f):
                    return []
            except Exception as e:
                return [output_api.PresubmitError(str(e))]

    if potential_problems:
        return [
            output_api.PresubmitPromptWarning(
                'Discovered possible removal of preference registrations.\n\n'
                'Please make sure to properly deprecate preferences by clearing their\n'
                'value for a couple of milestones before finally removing the code.\n'
                'Otherwise data may stay in the preferences files forever. See\n'
                'Migrate*Prefs() in chrome/browser/prefs/browser_prefs.cc and\n'
                'chrome/browser/prefs/README.md for examples.\n'
                'This may be a false positive warning (e.g. if you move preference\n'
                'registrations to a different place).\n', potential_problems)
        ]
    return []


def CheckConsistentGrdChanges(input_api, output_api):
    """Changes to GRD files must be consistent for tools to read them."""
    changed_grds = input_api.AffectedFiles(
        include_deletes=False,
        file_filter=lambda f: f.LocalPath().endswith(('.grd')))
    errors = []
    invalid_file_regexes = [(input_api.re.compile(matcher), msg)
                            for matcher, msg in _INVALID_GRD_FILE_LINE]
    for grd in changed_grds:
        for i, line in enumerate(grd.NewContents()):
            for matcher, msg in invalid_file_regexes:
                if matcher.search(line):
                    errors.append(
                        output_api.PresubmitError(
                            'Problem on {grd}:{i} - {msg}'.format(
                                grd=grd.LocalPath(), i=i + 1, msg=msg)))
    return errors


def CheckAssertAshOnlyCode(input_api, output_api):
    """Errors if a BUILD.gn file in an ash/ directory doesn't include
    assert(is_chromeos_ash).
    """

    def FileFilter(affected_file):
        """Includes directories known to be Ash only."""
        return input_api.FilterSourceFile(
            affected_file,
            files_to_check=(
                r'^ash/.*BUILD\.gn',  # Top-level src/ash/.
                r'.*/ash/.*BUILD\.gn'),  # Any path component.
            files_to_skip=(input_api.DEFAULT_FILES_TO_SKIP))

    errors = []
    pattern = input_api.re.compile(r'assert\(is_chromeos_ash')
    for f in input_api.AffectedFiles(include_deletes=False,
                                     file_filter=FileFilter):
        if (not pattern.search(input_api.ReadFile(f))):
            errors.append(
                output_api.PresubmitError(
                    'Please add assert(is_chromeos_ash) to %s. If that\'s not '
                    'possible, please create and issue and add a comment such '
                    'as:\n  # TODO(crbug.com/XXX): add '
                    'assert(is_chromeos_ash) when ...' % f.LocalPath()))
    return errors


def _IsMiraclePtrDisallowed(input_api, affected_file):
    path = affected_file.LocalPath()
    if not _IsCPlusPlusFile(input_api, path):
        return False

    # Renderer-only code is generally allowed to use MiraclePtr. These
    # directories, however, are specifically disallowed, for perf reasons.
    if ("third_party/blink/renderer/core/" in path
            or "third_party/blink/renderer/platform/heap/" in path
            or "third_party/blink/renderer/platform/wtf/" in path
            or "third_party/blink/renderer/platform/fonts/" in path):
        return True

    # The below paths are an explicitly listed subset of Renderer-only code,
    # because the plan is to Oilpanize it.
    # TODO(crbug.com/330759291): Remove once Oilpanization is completed or
    # abandoned.
    if ("third_party/blink/renderer/core/paint/" in path
            or "third_party/blink/renderer/platform/graphics/compositing/" in path
            or "third_party/blink/renderer/platform/graphics/paint/" in path):
        return True

    # We assume that everything else may be used outside of Renderer processes.
    return False

# TODO(crbug.com/40206238): Remove these checks, once they are replaced
# by the Chromium Clang Plugin (which will be preferable because it will
# 1) report errors earlier - at compile-time and 2) cover more rules).
def CheckRawPtrUsage(input_api, output_api):
    """Rough checks that raw_ptr<T> usage guidelines are followed."""
    errors = []
    # The regex below matches "raw_ptr<" following a word boundary, but not in a
    # C++ comment.
    raw_ptr_matcher = input_api.re.compile(r'^((?!//).)*\braw_ptr<')
    file_filter = lambda f: _IsMiraclePtrDisallowed(input_api, f)
    for f, line_num, line in input_api.RightHandSideLines(file_filter):
        if raw_ptr_matcher.search(line):
            errors.append(
                output_api.PresubmitError(
                    'Problem on {path}:{line} - '\
                    'raw_ptr<T> should not be used in this renderer code '\
                    '(as documented in the "Pointers to unprotected memory" '\
                    'section in //base/memory/raw_ptr.md)'.format(
                        path=f.LocalPath(), line=line_num)))
    return errors

def CheckAdvancedMemorySafetyChecksUsage(input_api, output_api):
    """Checks that ADVANCED_MEMORY_SAFETY_CHECKS() macro is neither added nor
    removed as it is managed by the memory safety team internally.
    Do not add / remove it manually."""
    paths = set([])
    # The regex below matches "ADVANCED_MEMORY_SAFETY_CHECKS(" following a word
    # boundary, but not in a C++ comment.
    macro_matcher = input_api.re.compile(
        r'^((?!//).)*\bADVANCED_MEMORY_SAFETY_CHECKS\(', input_api.re.MULTILINE)
    for f in input_api.AffectedFiles():
        if not _IsCPlusPlusFile(input_api, f.LocalPath()):
            continue
        if macro_matcher.search(f.GenerateScmDiff()):
            paths.add(f.LocalPath())
    if not paths:
        return []
    return [output_api.PresubmitPromptWarning(
              'ADVANCED_MEMORY_SAFETY_CHECKS() macro is managed by ' \
              'the memory safety team (chrome-memory-safety@). ' \
              'Please contact us to add/delete the uses of the macro.',
              paths)]

def CheckPythonShebang(input_api, output_api):
    """Checks that python scripts use #!/usr/bin/env instead of hardcoding a
    system-wide python.
    """
    errors = []
    sources = lambda affected_file: input_api.FilterSourceFile(
        affected_file,
        files_to_skip=((_THIRD_PARTY_EXCEPT_BLINK,
                        r'third_party/blink/web_tests/external/') + input_api.
                       DEFAULT_FILES_TO_SKIP),
        files_to_check=[r'.*\.py$'])
    for f in input_api.AffectedSourceFiles(sources):
        for line_num, line in f.ChangedContents():
            if line_num == 1 and line.startswith('#!/usr/bin/python'):
                errors.append(f.LocalPath())
            break

    result = []
    for file in errors:
        result.append(
            output_api.PresubmitError(
                "Please use '#!/usr/bin/env python/2/3' as the shebang of %s" %
                file))
    return result


def CheckBatchAnnotation(input_api, output_api):
    """Checks that tests have either @Batch or @DoNotBatch annotation. If this
    is not an instrumentation test, disregard."""

    batch_annotation = input_api.re.compile(r'^\s*@Batch')
    do_not_batch_annotation = input_api.re.compile(r'^\s*@DoNotBatch')
    robolectric_test = input_api.re.compile(r'[rR]obolectric')
    test_class_declaration = input_api.re.compile(r'^\s*public\sclass.*Test')
    uiautomator_test = input_api.re.compile(r'[uU]i[aA]utomator')
    test_annotation_declaration = input_api.re.compile(r'^\s*public\s@interface\s.*{')

    missing_annotation_errors = []
    extra_annotation_errors = []

    def _FilterFile(affected_file):
        return input_api.FilterSourceFile(
            affected_file,
            files_to_skip=input_api.DEFAULT_FILES_TO_SKIP,
            files_to_check=[r'.*Test\.java$'])

    for f in input_api.AffectedSourceFiles(_FilterFile):
        batch_matched = None
        do_not_batch_matched = None
        is_instrumentation_test = True
        test_annotation_declaration_matched = None
        for line in f.NewContents():
            if robolectric_test.search(line) or uiautomator_test.search(line):
                # Skip Robolectric and UiAutomator tests.
                is_instrumentation_test = False
                break
            if not batch_matched:
                batch_matched = batch_annotation.search(line)
            if not do_not_batch_matched:
                do_not_batch_matched = do_not_batch_annotation.search(line)
            test_class_declaration_matched = test_class_declaration.search(
                line)
            test_annotation_declaration_matched = test_annotation_declaration.search(line)
            if test_class_declaration_matched or test_annotation_declaration_matched:
                break
        if test_annotation_declaration_matched:
            continue
        if (is_instrumentation_test and
            not batch_matched and
            not do_not_batch_matched):
            missing_annotation_errors.append(str(f.LocalPath()))
        if (not is_instrumentation_test and
            (batch_matched or
             do_not_batch_matched)):
            extra_annotation_errors.append(str(f.LocalPath()))

    results = []

    if missing_annotation_errors:
        results.append(
            output_api.PresubmitPromptWarning(
                """
A change was made to an on-device test that has neither been annotated with
@Batch nor @DoNotBatch. If this is a new test, please add the annotation. If
this is an existing test, please consider adding it if you are sufficiently
familiar with the test (but do so as a separate change).

See https://source.chromium.org/chromium/chromium/src/+/main:docs/testing/batching_instrumentation_tests.md
""", missing_annotation_errors))
    if extra_annotation_errors:
        results.append(
            output_api.PresubmitPromptWarning(
                """
Robolectric tests do not need a @Batch or @DoNotBatch annotations.
""", extra_annotation_errors))

    return results


def CheckMockAnnotation(input_api, output_api):
    """Checks that we have annotated all Mockito.mock()-ed or Mockito.spy()-ed
    classes with @Mock or @Spy. If this is not an instrumentation test,
    disregard."""

    # This is just trying to be approximately correct. We are not writing a
    # Java parser, so special cases like statically importing mock() then
    # calling an unrelated non-mockito spy() function will cause a false
    # positive.
    package_name = input_api.re.compile(r'^package\s+(\w+(?:\.\w+)+);')
    mock_static_import = input_api.re.compile(
        r'^import\s+static\s+org.mockito.Mockito.(?:mock|spy);')
    import_class = input_api.re.compile(r'import\s+((?:\w+\.)+)(\w+);')
    mock_annotation = input_api.re.compile(r'^\s*@(?:Mock|Spy)')
    field_type = input_api.re.compile(r'(\w+)(?:<\w+>)?\s+\w+\s*(?:;|=)')
    mock_or_spy_function_call = r'(?:mock|spy)\(\s*(?:new\s*)?(\w+)(?:\.class|\()'
    fully_qualified_mock_function = input_api.re.compile(
        r'Mockito\.' + mock_or_spy_function_call)
    statically_imported_mock_function = input_api.re.compile(
        r'\W' + mock_or_spy_function_call)
    robolectric_test = input_api.re.compile(r'[rR]obolectric')
    uiautomator_test = input_api.re.compile(r'[uU]i[aA]utomator')

    def _DoClassLookup(class_name, class_name_map, package):
        found = class_name_map.get(class_name)
        if found is not None:
            return found
        else:
            return package + '.' + class_name

    def _FilterFile(affected_file):
        return input_api.FilterSourceFile(
            affected_file,
            files_to_skip=input_api.DEFAULT_FILES_TO_SKIP,
            files_to_check=[r'.*Test\.java$'])

    mocked_by_function_classes = set()
    mocked_by_annotation_classes = set()
    class_to_filename = {}
    for f in input_api.AffectedSourceFiles(_FilterFile):
        mock_function_regex = fully_qualified_mock_function
        next_line_is_annotated = False
        fully_qualified_class_map = {}
        package = None

        for line in f.NewContents():
            if robolectric_test.search(line) or uiautomator_test.search(line):
                # Skip Robolectric and UiAutomator tests.
                break

            m = package_name.search(line)
            if m:
                package = m.group(1)
                continue

            if mock_static_import.search(line):
                mock_function_regex = statically_imported_mock_function
                continue

            m = import_class.search(line)
            if m:
                fully_qualified_class_map[m.group(2)] = m.group(1) + m.group(2)
                continue

            if next_line_is_annotated:
                next_line_is_annotated = False
                fully_qualified_class = _DoClassLookup(
                    field_type.search(line).group(1), fully_qualified_class_map,
                    package)
                mocked_by_annotation_classes.add(fully_qualified_class)
                continue

            if mock_annotation.search(line):
                field_type_search = field_type.search(line)
                if field_type_search:
                    fully_qualified_class = _DoClassLookup(
                        field_type_search.group(1), fully_qualified_class_map,
                        package)
                    mocked_by_annotation_classes.add(fully_qualified_class)
                else:
                    next_line_is_annotated = True
                continue

            m = mock_function_regex.search(line)
            if m:
                fully_qualified_class = _DoClassLookup(m.group(1),
                    fully_qualified_class_map, package)
                # Skipping builtin classes, since they don't get optimized.
                if fully_qualified_class.startswith(
                        'android.') or fully_qualified_class.startswith(
                            'java.'):
                    continue
                class_to_filename[fully_qualified_class] = str(f.LocalPath())
                mocked_by_function_classes.add(fully_qualified_class)

    results = []
    missed_classes = mocked_by_function_classes - mocked_by_annotation_classes
    if missed_classes:
        error_locations = []
        for c in missed_classes:
            error_locations.append(c + ' in ' + class_to_filename[c])
        results.append(
            output_api.PresubmitPromptWarning(
                """
Mockito.mock()/spy() cause issues with our Java optimizer. You have 3 options:
1) If the mocked variable can be a class member, annotate the member with
   @Mock/@Spy.
2) If the mocked variable cannot be a class member, create a dummy member
   variable of that type, annotated with @Mock/@Spy. This dummy does not need
   to be used or initialized in any way.
3) If the mocked type is definitely not going to be optimized, whether it's a
   builtin type which we don't ship, or a class you know R8 will treat
   specially, you can ignore this warning.
""", error_locations))

    return results

def CheckNoJsInIos(input_api, output_api):
    """Checks to make sure that JavaScript files are not used on iOS."""

    def _FilterFile(affected_file):
        return input_api.FilterSourceFile(
            affected_file,
            files_to_skip=input_api.DEFAULT_FILES_TO_SKIP +
                (r'^ios/third_party/*', r'^ios/tools/*', r'^third_party/*',
                r'^components/autofill/ios/form_util/resources/*'),
            files_to_check=[r'^ios/.*\.js$', r'.*/ios/.*\.js$'])

    deleted_files = []

    # Collect filenames of all removed JS files.
    for f in input_api.AffectedFiles(file_filter=_FilterFile):
        local_path = f.LocalPath()

        if input_api.os_path.splitext(local_path)[1] == '.js' and f.Action() == 'D':
            deleted_files.append(input_api.os_path.basename(local_path))

    error_paths = []
    moved_paths = []
    warning_paths = []

    for f in input_api.AffectedSourceFiles(_FilterFile):
        local_path = f.LocalPath()

        if input_api.os_path.splitext(local_path)[1] == '.js':
            if f.Action() == 'A':
                if input_api.os_path.basename(local_path) in deleted_files:
                    # This script was probably moved rather than newly created.
                    # Present a warning instead of an error for these cases.
                    moved_paths.append(local_path)
                else:
                    error_paths.append(local_path)
            elif f.Action() != 'D':
                warning_paths.append(local_path)

    results = []

    if warning_paths:
        results.append(output_api.PresubmitPromptWarning(
            'TypeScript is now fully supported for iOS feature scripts. '
            'Consider converting JavaScript files to TypeScript. See '
            '//ios/web/public/js_messaging/README.md for more details.',
            warning_paths))

    if moved_paths:
        results.append(output_api.PresubmitPromptWarning(
            'Do not use JavaScript on iOS for new files as TypeScript is '
            'fully supported. (If this is a moved file, you may leave the '
            'script unconverted.) See //ios/web/public/js_messaging/README.md '
            'for help using scripts on iOS.', moved_paths))

    if error_paths:
        results.append(output_api.PresubmitError(
            'Do not use JavaScript on iOS as TypeScript is fully supported. '
            'See //ios/web/public/js_messaging/README.md for help using '
            'scripts on iOS.', error_paths))

    return results

def CheckLibcxxRevisionsMatch(input_api, output_api):
    """Check to make sure the libc++ version matches across deps files."""
    # Disable check for changes to sub-repositories.
    if input_api.PresubmitLocalPath() != input_api.change.RepositoryRoot():
        return []

    DEPS_FILES = [ 'DEPS', 'buildtools/deps_revisions.gni' ]

    file_filter = lambda f: f.LocalPath().replace(
            input_api.os_path.sep, '/') in DEPS_FILES
    changed_deps_files = input_api.AffectedFiles(file_filter=file_filter)
    if not changed_deps_files:
        return []

    def LibcxxRevision(file):
        file = input_api.os_path.join(input_api.PresubmitLocalPath(),
                                      *file.split('/'))
        return input_api.re.search(
                r'libcxx_revision.*[:=].*[\'"](\w+)[\'"]',
                input_api.ReadFile(file)).group(1)

    if len(set([LibcxxRevision(f) for f in DEPS_FILES])) == 1:
        return []

    return [output_api.PresubmitError(
        'libcxx_revision not equal across %s' % ', '.join(DEPS_FILES),
        changed_deps_files)]


def CheckDanglingUntriaged(input_api, output_api):
    """Warn developers adding DanglingUntriaged raw_ptr."""

    # Ignore during git presubmit --all.
    #
    # This would be too costly, because this would check every lines of every
    # C++ files. Check from _BANNED_CPP_FUNCTIONS are also reading the whole
    # source code, but only once to apply every checks. It seems the bots like
    # `win-presubmit` are particularly sensitive to reading the files. Adding
    # this check caused the bot to run 2x longer. See https://crbug.com/1486612.
    if input_api.no_diffs:
        return []

    def FilterFile(file):
        return input_api.FilterSourceFile(
            file,
            files_to_check=[r".*\.(h|cc|cpp|cxx|m|mm)$"],
            files_to_skip=[r"^base/allocator.*"],
        )

    count = 0
    for f in input_api.AffectedFiles(file_filter=FilterFile):
        count -= sum([l.count("DanglingUntriaged") for l in f.OldContents()])
        count += sum([l.count("DanglingUntriaged") for l in f.NewContents()])

    # Most likely, nothing changed:
    if count == 0:
        return []

    # Congrats developers for improving it:
    if count < 0:
        message = f"DanglingUntriaged pointers removed: {-count}\nThank you!"
        return [output_api.PresubmitNotifyResult(message)]

    # Check for 'DanglingUntriaged-notes' in the description:
    notes_regex = input_api.re.compile("DanglingUntriaged-notes[:=]")
    if any(
            notes_regex.match(line)
            for line in input_api.change.DescriptionText().splitlines()):
        return []

    # Check for DanglingUntriaged-notes in the git footer:
    if input_api.change.GitFootersFromDescription().get(
            "DanglingUntriaged-notes", []):
        return []

    message = (
        "Unexpected new occurrences of `DanglingUntriaged` detected. Please\n" +
        "avoid adding new ones\n" +
        "\n" +
        "See documentation:\n" +
        "https://chromium.googlesource.com/chromium/src/+/main/docs/dangling_ptr.md\n" +
        "\n" +
        "See also the guide to fix dangling pointers:\n" +
        "https://chromium.googlesource.com/chromium/src/+/main/docs/dangling_ptr_guide.md\n" +
        "\n" +
        "To disable this warning, please add in the commit description:\n" +
        "DanglingUntriaged-notes: <rationale for new untriaged dangling " +
        "pointers>"
    )
    return [output_api.PresubmitPromptWarning(message)]

def CheckInlineConstexprDefinitionsInHeaders(input_api, output_api):
    """Checks that non-static constexpr definitions in headers are inline."""
    # In a properly formatted file, constexpr definitions inside classes or
    # structs will have additional whitespace at the beginning of the line.
    # The pattern looks for variables initialized as constexpr kVar = ...; or
    # constexpr kVar{...};
    # The pattern does not match expressions that have braces in kVar to avoid
    # matching constexpr functions.
    pattern = input_api.re.compile(r'^constexpr (?!inline )[^\(\)]*[={]')
    attribute_pattern = input_api.re.compile(r'(\[\[[a-zA-Z_:]+\]\]|[A-Z]+[A-Z_]+) ')
    problems = []
    for f in input_api.AffectedFiles():
        if not _IsCPlusPlusHeaderFile(input_api, f.LocalPath()):
            continue

        for line_number, line in f.ChangedContents():
            line = attribute_pattern.sub('', line)
            if pattern.search(line):
                problems.append(
                    f"{f.LocalPath()}: {line_number}\n    {line}")

    if problems:
        return [
            output_api.PresubmitPromptWarning(
                'Consider inlining constexpr variable definitions in headers '
                'outside of classes to avoid unnecessary copies of the '
                'constant. See https://abseil.io/tips/168 for more details.',
                problems)
        ]
    else:
        return []

def CheckTodoBugReferences(input_api, output_api):
    """Checks that bugs in TODOs use updated issue tracker IDs."""

    files_to_skip = ['PRESUBMIT_test.py']

    def _FilterFile(affected_file):
        return input_api.FilterSourceFile(
            affected_file,
            files_to_skip=files_to_skip)

    # Monorail bug IDs are all less than or equal to 1524553 so check that all
    # bugs in TODOs are greater than that value.
    pattern = input_api.re.compile(r'.*TODO\([^\)0-9]*([0-9]+)\).*')
    problems = []
    for f in input_api.AffectedSourceFiles(_FilterFile):
        for line_number, line in f.ChangedContents():
            match = pattern.match(line)
            if match and int(match.group(1)) <= 1524553:
                problems.append(
                    f"{f.LocalPath()}: {line_number}\n    {line}")

    if problems:
        return [
            output_api.PresubmitPromptWarning(
                'TODOs should use the new Chromium Issue Tracker IDs which can '
                'be found by navigating to the bug. See '
                'https://crbug.com/336778624 for more details.',
                problems)
        ]
    else:
        return []
