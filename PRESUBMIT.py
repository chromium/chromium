# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Top-level presubmit script for Chromium.

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details about the presubmit API built into depot_tools.
"""


_EXCLUDED_PATHS = (
    r"^native_client_sdk[\\/]src[\\/]build_tools[\\/]make_rules.py",
    r"^native_client_sdk[\\/]src[\\/]build_tools[\\/]make_simple.py",
    r"^native_client_sdk[\\/]src[\\/]tools[\\/].*.mk",
    r"^net[\\/]tools[\\/]spdyshark[\\/].*",
    r"^skia[\\/].*",
    r"^third_party[\\/](WebKit|blink)[\\/].*",
    r"^third_party[\\/]breakpad[\\/].*",
    r"^v8[\\/].*",
    r".*MakeFile$",
    r".+_autogen\.h$",
    r".+[\\/]pnacl_shim\.c$",
    r"^gpu[\\/]config[\\/].*_list_json\.cc$",
    r"^chrome[\\/]browser[\\/]resources[\\/]pdf[\\/]index.js",
    r"tools[\\/]md_browser[\\/].*\.css$",
    # Test pages for Maps telemetry tests.
    r"tools[\\/]perf[\\/]page_sets[\\/]maps_perf_test.*",
    # Test pages for WebRTC telemetry tests.
    r"tools[\\/]perf[\\/]page_sets[\\/]webrtc_cases.*",
)


# Fragment of a regular expression that matches C++ and Objective-C++
# implementation files.
_IMPLEMENTATION_EXTENSIONS = r'\.(cc|cpp|cxx|mm)$'


# Fragment of a regular expression that matches C++ and Objective-C++
# header files.
_HEADER_EXTENSIONS = r'\.(h|hpp|hxx)$'


# Regular expression that matches code only used for test binaries
# (best effort).
_TEST_CODE_EXCLUDED_PATHS = (
    r'.*[\\/](fake_|test_|mock_).+%s' % _IMPLEMENTATION_EXTENSIONS,
    r'.+_test_(base|support|util)%s' % _IMPLEMENTATION_EXTENSIONS,
    r'.+_(api|browser|eg|int|perf|pixel|unit|ui)?test(_[a-z]+)?%s' %
        _IMPLEMENTATION_EXTENSIONS,
    r'.+profile_sync_service_harness%s' % _IMPLEMENTATION_EXTENSIONS,
    r'.*[\\/](test|tool(s)?)[\\/].*',
    # content_shell is used for running layout tests.
    r'content[\\/]shell[\\/].*',
    # Non-production example code.
    r'mojo[\\/]examples[\\/].*',
    # Launcher for running iOS tests on the simulator.
    r'testing[\\/]iossim[\\/]iossim\.mm$',
)


_TEST_ONLY_WARNING = (
    'You might be calling functions intended only for testing from\n'
    'production code.  It is OK to ignore this warning if you know what\n'
    'you are doing, as the heuristics used to detect the situation are\n'
    'not perfect.  The commit queue will not block on this warning.')


_INCLUDE_ORDER_WARNING = (
    'Your #include order seems to be broken. Remember to use the right '
    'collation (LC_COLLATE=C) and check\nhttps://google.github.io/styleguide/'
    'cppguide.html#Names_and_Order_of_Includes')


_BANNED_JAVA_FUNCTIONS = (
    (
      'StrictMode.allowThreadDiskReads()',
      (
       'Prefer using StrictModeContext.allowDiskReads() to using StrictMode '
       'directly.',
      ),
      False,
    ),
    (
      'StrictMode.allowThreadDiskWrites()',
      (
       'Prefer using StrictModeContext.allowDiskWrites() to using StrictMode '
       'directly.',
      ),
      False,
    ),
)

_BANNED_OBJC_FUNCTIONS = (
    (
      'addTrackingRect:',
      (
       'The use of -[NSView addTrackingRect:owner:userData:assumeInside:] is'
       'prohibited. Please use CrTrackingArea instead.',
       'http://dev.chromium.org/developers/coding-style/cocoa-dos-and-donts',
      ),
      False,
    ),
    (
      r'/NSTrackingArea\W',
      (
       'The use of NSTrackingAreas is prohibited. Please use CrTrackingArea',
       'instead.',
       'http://dev.chromium.org/developers/coding-style/cocoa-dos-and-donts',
      ),
      False,
    ),
    (
      'convertPointFromBase:',
      (
       'The use of -[NSView convertPointFromBase:] is almost certainly wrong.',
       'Please use |convertPoint:(point) fromView:nil| instead.',
       'http://dev.chromium.org/developers/coding-style/cocoa-dos-and-donts',
      ),
      True,
    ),
    (
      'convertPointToBase:',
      (
       'The use of -[NSView convertPointToBase:] is almost certainly wrong.',
       'Please use |convertPoint:(point) toView:nil| instead.',
       'http://dev.chromium.org/developers/coding-style/cocoa-dos-and-donts',
      ),
      True,
    ),
    (
      'convertRectFromBase:',
      (
       'The use of -[NSView convertRectFromBase:] is almost certainly wrong.',
       'Please use |convertRect:(point) fromView:nil| instead.',
       'http://dev.chromium.org/developers/coding-style/cocoa-dos-and-donts',
      ),
      True,
    ),
    (
      'convertRectToBase:',
      (
       'The use of -[NSView convertRectToBase:] is almost certainly wrong.',
       'Please use |convertRect:(point) toView:nil| instead.',
       'http://dev.chromium.org/developers/coding-style/cocoa-dos-and-donts',
      ),
      True,
    ),
    (
      'convertSizeFromBase:',
      (
       'The use of -[NSView convertSizeFromBase:] is almost certainly wrong.',
       'Please use |convertSize:(point) fromView:nil| instead.',
       'http://dev.chromium.org/developers/coding-style/cocoa-dos-and-donts',
      ),
      True,
    ),
    (
      'convertSizeToBase:',
      (
       'The use of -[NSView convertSizeToBase:] is almost certainly wrong.',
       'Please use |convertSize:(point) toView:nil| instead.',
       'http://dev.chromium.org/developers/coding-style/cocoa-dos-and-donts',
      ),
      True,
    ),
    (
      r"/\s+UTF8String\s*]",
      (
       'The use of -[NSString UTF8String] is dangerous as it can return null',
       'even if |canBeConvertedToEncoding:NSUTF8StringEncoding| returns YES.',
       'Please use |SysNSStringToUTF8| instead.',
      ),
      True,
    ),
    (
      r'__unsafe_unretained',
      (
        'The use of __unsafe_unretained is almost certainly wrong, unless',
        'when interacting with NSFastEnumeration or NSInvocation.',
        'Please use __weak in files build with ARC, nothing otherwise.',
      ),
      False,
    ),
)

_BANNED_IOS_OBJC_FUNCTIONS = (
    (
      r'/\bTEST[(]',
      (
        'TEST() macro should not be used in Objective-C++ code as it does not ',
        'drain the autorelease pool at the end of the test. Use TEST_F() ',
        'macro instead with a fixture inheriting from PlatformTest (or a ',
        'typedef).'
      ),
      True,
    ),
    (
      r'/\btesting::Test\b',
      (
        'testing::Test should not be used in Objective-C++ code as it does ',
        'not drain the autorelease pool at the end of the test. Use ',
        'PlatformTest instead.'
      ),
      True,
    ),
)


_BANNED_CPP_FUNCTIONS = (
    # Make sure that gtest's FRIEND_TEST() macro is not used; the
    # FRIEND_TEST_ALL_PREFIXES() macro from base/gtest_prod_util.h should be
    # used instead since that allows for FLAKY_ and DISABLED_ prefixes.
    (
      r'\bNULL\b',
      (
       'New code should not use NULL. Use nullptr instead.',
      ),
      True,
      (),
    ),
    (
      'FRIEND_TEST(',
      (
       'Chromium code should not use gtest\'s FRIEND_TEST() macro. Include',
       'base/gtest_prod_util.h and use FRIEND_TEST_ALL_PREFIXES() instead.',
      ),
      False,
      (),
    ),
    (
      r'XSelectInput|CWEventMask|XCB_CW_EVENT_MASK',
      (
       'Chrome clients wishing to select events on X windows should use',
       'ui::XScopedEventSelector.  It is safe to ignore this warning only if',
       'you are selecting events from the GPU process, or if you are using',
       'an XDisplay other than gfx::GetXDisplay().',
      ),
      True,
      (
        r"^ui[\\/]gl[\\/].*\.cc$",
        r"^media[\\/]gpu[\\/].*\.cc$",
        r"^gpu[\\/].*\.cc$",
      ),
    ),
    (
      r'XInternAtom|xcb_intern_atom',
      (
       'Use gfx::GetAtom() instead of interning atoms directly.',
      ),
      True,
      (
        r"^gpu[\\/]ipc[\\/]service[\\/]gpu_watchdog_thread\.cc$",
        r"^remoting[\\/]host[\\/]linux[\\/]x_server_clipboard\.cc$",
        r"^ui[\\/]gfx[\\/]x[\\/]x11_atom_cache\.cc$",
      ),
    ),
    (
      'setMatrixClip',
      (
        'Overriding setMatrixClip() is prohibited; ',
        'the base function is deprecated. ',
      ),
      True,
      (),
    ),
    (
      'SkRefPtr',
      (
        'The use of SkRefPtr is prohibited. ',
        'Please use sk_sp<> instead.'
      ),
      True,
      (),
    ),
    (
      'SkAutoRef',
      (
        'The indirect use of SkRefPtr via SkAutoRef is prohibited. ',
        'Please use sk_sp<> instead.'
      ),
      True,
      (),
    ),
    (
      'SkAutoTUnref',
      (
        'The use of SkAutoTUnref is dangerous because it implicitly ',
        'converts to a raw pointer. Please use sk_sp<> instead.'
      ),
      True,
      (),
    ),
    (
      'SkAutoUnref',
      (
        'The indirect use of SkAutoTUnref through SkAutoUnref is dangerous ',
        'because it implicitly converts to a raw pointer. ',
        'Please use sk_sp<> instead.'
      ),
      True,
      (),
    ),
    (
      r'/HANDLE_EINTR\(.*close',
      (
       'HANDLE_EINTR(close) is invalid. If close fails with EINTR, the file',
       'descriptor will be closed, and it is incorrect to retry the close.',
       'Either call close directly and ignore its return value, or wrap close',
       'in IGNORE_EINTR to use its return value. See http://crbug.com/269623'
      ),
      True,
      (),
    ),
    (
      r'/IGNORE_EINTR\((?!.*close)',
      (
       'IGNORE_EINTR is only valid when wrapping close. To wrap other system',
       'calls, use HANDLE_EINTR. See http://crbug.com/269623',
      ),
      True,
      (
        # Files that #define IGNORE_EINTR.
        r'^base[\\/]posix[\\/]eintr_wrapper\.h$',
        r'^ppapi[\\/]tests[\\/]test_broker\.cc$',
      ),
    ),
    (
      r'/v8::Extension\(',
      (
        'Do not introduce new v8::Extensions into the code base, use',
        'gin::Wrappable instead. See http://crbug.com/334679',
      ),
      True,
      (
        r'extensions[\\/]renderer[\\/]safe_builtins\.*',
      ),
    ),
    (
      '#pragma comment(lib,',
      (
        'Specify libraries to link with in build files and not in the source.',
      ),
      True,
      (
          r'^third_party[\\/]abseil-cpp[\\/].*',
      ),
    ),
    (
      r'/base::SequenceChecker\b',
      (
        'Consider using SEQUENCE_CHECKER macros instead of the class directly.',
      ),
      False,
      (),
    ),
    (
      r'/base::ThreadChecker\b',
      (
        'Consider using THREAD_CHECKER macros instead of the class directly.',
      ),
      False,
      (),
    ),
    (
      r'/(Time(|Delta|Ticks)|ThreadTicks)::FromInternalValue|ToInternalValue',
      (
        'base::TimeXXX::FromInternalValue() and ToInternalValue() are',
        'deprecated (http://crbug.com/634507). Please avoid converting away',
        'from the Time types in Chromium code, especially if any math is',
        'being done on time values. For interfacing with platform/library',
        'APIs, use FromMicroseconds() or InMicroseconds(), or one of the other',
        'type converter methods instead. For faking TimeXXX values (for unit',
        'testing only), use TimeXXX() + TimeDelta::FromMicroseconds(N). For',
        'other use cases, please contact base/time/OWNERS.',
      ),
      False,
      (),
    ),
    (
      'CallJavascriptFunctionUnsafe',
      (
        "Don't use CallJavascriptFunctionUnsafe() in new code. Instead, use",
        'AllowJavascript(), OnJavascriptAllowed()/OnJavascriptDisallowed(),',
        'and CallJavascriptFunction(). See https://goo.gl/qivavq.',
      ),
      False,
      (
        r'^content[\\/]browser[\\/]webui[\\/]web_ui_impl\.(cc|h)$',
        r'^content[\\/]public[\\/]browser[\\/]web_ui\.h$',
        r'^content[\\/]public[\\/]test[\\/]test_web_ui\.(cc|h)$',
      ),
    ),
    (
      'leveldb::DB::Open',
      (
        'Instead of leveldb::DB::Open() use leveldb_env::OpenDB() from',
        'third_party/leveldatabase/env_chromium.h. It exposes databases to',
        "Chrome's tracing, making their memory usage visible.",
      ),
      True,
      (
        r'^third_party/leveldatabase/.*\.(cc|h)$',
      ),
    ),
    (
      'leveldb::NewMemEnv',
      (
        'Instead of leveldb::NewMemEnv() use leveldb_chrome::NewMemEnv() from',
        'third_party/leveldatabase/leveldb_chrome.h. It exposes environments',
        "to Chrome's tracing, making their memory usage visible.",
      ),
      True,
      (
        r'^third_party/leveldatabase/.*\.(cc|h)$',
      ),
    ),
    (
      'RunLoop::QuitCurrent',
      (
        'Please migrate away from RunLoop::QuitCurrent*() methods. Use member',
        'methods of a specific RunLoop instance instead.',
      ),
      False,
      (),
    ),
    (
      'base::ScopedMockTimeMessageLoopTaskRunner',
      (
        'ScopedMockTimeMessageLoopTaskRunner is deprecated. Prefer',
        'ScopedTaskEnvironment::MainThreadType::MOCK_TIME. There are still a',
        'few cases that may require a ScopedMockTimeMessageLoopTaskRunner',
        '(i.e. mocking the main MessageLoopForUI in browser_tests), but check',
        'with gab@ first if you think you need it)',
      ),
      False,
      (),
    ),
    (
      r'std::regex',
      (
        'Using std::regex adds unnecessary binary size to Chrome. Please use',
        're2::RE2 instead (crbug.com/755321)',
      ),
      True,
      (),
    ),
    (
      (r'/base::ThreadRestrictions::(ScopedAllowIO|AssertIOAllowed|'
       r'DisallowWaiting|AssertWaitAllowed|SetWaitAllowed|ScopedAllowWait)'),
      (
        'Use the new API in base/threading/thread_restrictions.h.',
      ),
      False,
      (),
    ),
    (
      r'/\bbase::Bind\(',
      (
          'Please consider using base::Bind{Once,Repeating} instead',
          'of base::Bind. (crbug.com/714018)',
      ),
      False,
      (),
    ),
    (
      r'/\bbase::Callback<',
      (
          'Please consider using base::{Once,Repeating}Callback instead',
          'of base::Callback. (crbug.com/714018)',
      ),
      False,
      (),
    ),
    (
      r'/\bbase::Closure\b',
      (
          'Please consider using base::{Once,Repeating}Closure instead',
          'of base::Closure. (crbug.com/714018)',
      ),
      False,
      (),
    ),
    (
      r'RunMessageLoop',
      (
          'RunMessageLoop is deprecated, use RunLoop instead.',
      ),
      False,
      (),
    ),
    (
      r'RunThisRunLoop',
      (
          'RunThisRunLoop is deprecated, use RunLoop directly instead.',
      ),
      False,
      (),
    ),
    (
      r'RunAllPendingInMessageLoop()',
      (
          "Prefer RunLoop over RunAllPendingInMessageLoop, please contact gab@",
          "if you're convinced you need this.",
      ),
      False,
      (),
    ),
    (
      r'RunAllPendingInMessageLoop(BrowserThread',
      (
          'RunAllPendingInMessageLoop is deprecated. Use RunLoop for',
          'BrowserThread::UI, TestBrowserThreadBundle::RunIOThreadUntilIdle',
          'for BrowserThread::IO, and prefer RunLoop::QuitClosure to observe',
          'async events instead of flushing threads.',
      ),
      False,
      (),
    ),
    (
      r'MessageLoopRunner',
      (
          'MessageLoopRunner is deprecated, use RunLoop instead.',
      ),
      False,
      (),
    ),
    (
      r'GetDeferredQuitTaskForRunLoop',
      (
          "GetDeferredQuitTaskForRunLoop shouldn't be needed, please contact",
          "gab@ if you found a use case where this is the only solution.",
      ),
      False,
      (),
    ),
    (
      'sqlite3_initialize',
      (
        'Instead of sqlite3_initialize, depend on //sql, ',
        '#include "sql/initialize.h" and use sql::EnsureSqliteInitialized().',
      ),
      True,
      (
        r'^sql/initialization\.(cc|h)$',
        r'^third_party/sqlite/.*\.(c|cc|h)$',
      ),
    ),
    (
      'net::URLFetcher',
      (
        'net::URLFetcher should no longer be used in content embedders. ',
        'Instead, use network::SimpleURLLoader instead, which supports ',
        'an out-of-process network stack. ',
        'net::URLFetcher may still be used in binaries that do not embed',
        'content.',
      ),
      False,
      (
        r'^ios[\\/].*\.(cc|h)$',
        r'.*[\\/]ios[\\/].*\.(cc|h)$',
        r'.*_ios\.(cc|h)$',
        r'^net[\\/].*\.(cc|h)$',
        r'.*[\\/]tools[\\/].*\.(cc|h)$',
      ),
    ),
    (
      r'/\barraysize\b',
      (
          "arraysize is deprecated, please use base::size(array) instead ",
          "(https://crbug.com/837308). ",
      ),
      False,
      (),
    ),
    (
      r'std::random_shuffle',
      (
        'std::random_shuffle is deprecated in C++14, and removed in C++17. Use',
        'base::RandomShuffle instead.'
      ),
      True,
      (),
    ),
    (
      'ios/web/public/test/http_server',
      (
        'web::HTTPserver is deprecated use net::EmbeddedTestServer instead.',
      ),
      False,
      (),
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
    r".*[\\/]BuildHooksAndroidImpl\.java",
    r".*[\\/]LicenseContentProvider\.java",
    r".*[\\/]PlatformServiceBridgeImpl.java",
    r".*chrome[\\\/]android[\\\/]feed[\\\/]dummy[\\\/].*\.java",
]

# These paths contain test data and other known invalid JSON files.
_KNOWN_INVALID_JSON_FILE_PATTERNS = [
    r'test[\\/]data[\\/]',
    r'^components[\\/]policy[\\/]resources[\\/]policy_templates\.json$',
    r'^third_party[\\/]protobuf[\\/]',
    r'^third_party[\\/]WebKit[\\/]LayoutTests[\\/]external[\\/]wpt[\\/]',
    r'^third_party[\\/]blink[\\/]renderer[\\/]devtools[\\/]protocol\.json$',
]


_VALID_OS_MACROS = (
    # Please keep sorted.
    'OS_AIX',
    'OS_ANDROID',
    'OS_ASMJS',
    'OS_BSD',
    'OS_CAT',       # For testing.
    'OS_CHROMEOS',
    'OS_FREEBSD',
    'OS_FUCHSIA',
    'OS_IOS',
    'OS_LINUX',
    'OS_MACOSX',
    'OS_NACL',
    'OS_NACL_NONSFI',
    'OS_NACL_SFI',
    'OS_NETBSD',
    'OS_OPENBSD',
    'OS_POSIX',
    'OS_QNX',
    'OS_SOLARIS',
    'OS_WIN',
)


_ANDROID_SPECIFIC_PYDEPS_FILES = [
    'android_webview/tools/run_cts.pydeps',
    'base/android/jni_generator/jni_generator.pydeps',
    'base/android/jni_generator/jni_registration_generator.pydeps',
    'build/android/gyp/aar.pydeps',
    'build/android/gyp/aidl.pydeps',
    'build/android/gyp/apkbuilder.pydeps',
    'build/android/gyp/app_bundle_to_apks.pydeps',
    'build/android/gyp/bytecode_processor.pydeps',
    'build/android/gyp/compile_resources.pydeps',
    'build/android/gyp/create_bundle_wrapper_script.pydeps',
    'build/android/gyp/copy_ex.pydeps',
    'build/android/gyp/create_app_bundle.pydeps',
    'build/android/gyp/create_apk_operations_script.pydeps',
    'build/android/gyp/create_dist_jar.pydeps',
    'build/android/gyp/create_java_binary_script.pydeps',
    'build/android/gyp/create_stack_script.pydeps',
    'build/android/gyp/create_test_runner_script.pydeps',
    'build/android/gyp/create_tool_wrapper.pydeps',
    'build/android/gyp/desugar.pydeps',
    'build/android/gyp/dex.pydeps',
    'build/android/gyp/dist_aar.pydeps',
    'build/android/gyp/emma_instr.pydeps',
    'build/android/gyp/filter_zip.pydeps',
    'build/android/gyp/gcc_preprocess.pydeps',
    'build/android/gyp/generate_proguarded_module_jar.pydeps',
    'build/android/gyp/ijar.pydeps',
    'build/android/gyp/java_cpp_enum.pydeps',
    'build/android/gyp/javac.pydeps',
    'build/android/gyp/jinja_template.pydeps',
    'build/android/gyp/lint.pydeps',
    'build/android/gyp/main_dex_list.pydeps',
    'build/android/gyp/merge_jar_info_files.pydeps',
    'build/android/gyp/merge_manifest.pydeps',
    'build/android/gyp/prepare_resources.pydeps',
    'build/android/gyp/proguard.pydeps',
    'build/android/gyp/write_build_config.pydeps',
    'build/android/gyp/write_ordered_libraries.pydeps',
    'build/android/incremental_install/generate_android_manifest.pydeps',
    'build/android/incremental_install/write_installer_json.pydeps',
    'build/android/resource_sizes.pydeps',
    'build/android/test_runner.pydeps',
    'build/android/test_wrapper/logdog_wrapper.pydeps',
    'build/protoc_java.pydeps',
    'build/secondary/third_party/android_platform/'
        'development/scripts/stack.pydeps',
    'net/tools/testserver/testserver.pydeps',
]


_GENERIC_PYDEPS_FILES = [
    'chrome/test/chromedriver/test/run_py_tests.pydeps',
    'chrome/test/chromedriver/log_replay/client_replay_unittest.pydeps',
    'tools/binary_size/supersize.pydeps',
]


_ALL_PYDEPS_FILES = _ANDROID_SPECIFIC_PYDEPS_FILES + _GENERIC_PYDEPS_FILES


# Bypass the AUTHORS check for these accounts.
_KNOWN_ROBOTS = set(
    '%s-chromium-autoroll@skia-buildbots.google.com.iam.gserviceaccount.com' % s
    for s in ('afdo', 'angle', 'catapult', 'chromite', 'depot-tools',
              'fuchsia-sdk', 'nacl', 'pdfium', 'perfetto', 'skia',
              'spirv', 'src-internal', 'webrtc')
  ) | set('%s@appspot.gserviceaccount.com' % s for s in ('findit-for-me',)
  ) | set('%s@developer.gserviceaccount.com' % s for s in ('3su6n15k.default',)
  ) | set('%s@chops-service-accounts.iam.gserviceaccount.com' % s
          for s in ('v8-ci-autoroll-builder',)
  ) | set('%s@skia-public.iam.gserviceaccount.com' % s
          for s in ('chromium-autoroll',)
  ) | set('%s@skia-corp.google.com.iam.gserviceaccount.com' % s
          for s in ('chromium-internal-autoroll',))


def _CheckNoProductionCodeUsingTestOnlyFunctions(input_api, output_api):
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
  inclusion_pattern = input_api.re.compile(r'(%s)\s*\(' % base_function_pattern)
  comment_pattern = input_api.re.compile(r'//.*(%s)' % base_function_pattern)
  exclusion_pattern = input_api.re.compile(
    r'::[A-Za-z0-9_]+(%s)|(%s)[^;]+\{' % (
      base_function_pattern, base_function_pattern))

  def FilterFile(affected_file):
    black_list = (_EXCLUDED_PATHS +
                  _TEST_CODE_EXCLUDED_PATHS +
                  input_api.DEFAULT_BLACK_LIST)
    return input_api.FilterSourceFile(
      affected_file,
      white_list=file_inclusion_pattern,
      black_list=black_list)

  problems = []
  for f in input_api.AffectedSourceFiles(FilterFile):
    local_path = f.LocalPath()
    for line_number, line in f.ChangedContents():
      if (inclusion_pattern.search(line) and
          not comment_pattern.search(line) and
          not exclusion_pattern.search(line)):
        problems.append(
          '%s:%d\n    %s' % (local_path, line_number, line.strip()))

  if problems:
    return [output_api.PresubmitPromptOrNotify(_TEST_ONLY_WARNING, problems)]
  else:
    return []


def _CheckNoProductionCodeUsingTestOnlyFunctionsJava(input_api, output_api):
  """This is a simplified version of
  _CheckNoProductionCodeUsingTestOnlyFunctions for Java files.
  """
  javadoc_start_re = input_api.re.compile(r'^\s*/\*\*')
  javadoc_end_re = input_api.re.compile(r'^\s*\*/')
  name_pattern = r'ForTest(s|ing)?'
  # Describes an occurrence of "ForTest*" inside a // comment.
  comment_re = input_api.re.compile(r'//.*%s' % name_pattern)
  # Catch calls.
  inclusion_re = input_api.re.compile(r'(%s)\s*\(' % name_pattern)
  # Ignore definitions. (Comments are ignored separately.)
  exclusion_re = input_api.re.compile(r'(%s)[^;]+\{' % name_pattern)

  problems = []
  sources = lambda x: input_api.FilterSourceFile(
    x,
    black_list=(('(?i).*test', r'.*\/junit\/')
                + input_api.DEFAULT_BLACK_LIST),
    white_list=[r'.*\.java$']
  )
  for f in input_api.AffectedFiles(include_deletes=False, file_filter=sources):
    local_path = f.LocalPath()
    is_inside_javadoc = False
    for line_number, line in f.ChangedContents():
      if is_inside_javadoc and javadoc_end_re.search(line):
        is_inside_javadoc = False
      if not is_inside_javadoc and javadoc_start_re.search(line):
        is_inside_javadoc = True
      if is_inside_javadoc:
        continue
      if (inclusion_re.search(line) and
          not comment_re.search(line) and
          not exclusion_re.search(line)):
        problems.append(
          '%s:%d\n    %s' % (local_path, line_number, line.strip()))

  if problems:
    return [output_api.PresubmitPromptOrNotify(_TEST_ONLY_WARNING, problems)]
  else:
    return []


def _CheckNoIOStreamInHeaders(input_api, output_api):
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
    return [output_api.PresubmitError(
        'Do not #include <iostream> in header files, since it inserts static '
        'initialization into every file including the header. Instead, '
        '#include <ostream>. See http://crbug.com/94794',
        files) ]
  return []

def _CheckNoStrCatRedefines(input_api, output_api):
  """Checks no windows headers with StrCat redefined are included directly."""
  files = []
  pattern_deny = input_api.re.compile(
      r'^#include\s*[<"](shlwapi|atlbase|propvarutil|sphelper).h[">]',
      input_api.re.MULTILINE)
  pattern_allow = input_api.re.compile(
      r'^#include\s"base/win/windows_defines.inc"',
      input_api.re.MULTILINE)
  for f in input_api.AffectedSourceFiles(input_api.FilterSourceFile):
    contents = input_api.ReadFile(f)
    if pattern_deny.search(contents) and not pattern_allow.search(contents):
      files.append(f.LocalPath())

  if len(files):
    return [output_api.PresubmitError(
        'Do not #include shlwapi.h, atlbase.h, propvarutil.h or sphelper.h '
        'directly since they pollute code with StrCat macro. Instead, '
        'include matching header from base/win. See http://crbug.com/856536',
        files) ]
  return []


def _CheckNoUNIT_TESTInSourceFiles(input_api, output_api):
  """Checks to make sure no source files use UNIT_TEST."""
  problems = []
  for f in input_api.AffectedFiles():
    if (not f.LocalPath().endswith(('.cc', '.mm'))):
      continue

    for line_num, line in f.ChangedContents():
      if 'UNIT_TEST ' in line or line.endswith('UNIT_TEST'):
        problems.append('    %s:%d' % (f.LocalPath(), line_num))

  if not problems:
    return []
  return [output_api.PresubmitPromptWarning('UNIT_TEST is only for headers.\n' +
      '\n'.join(problems))]

def _CheckNoDISABLETypoInTests(input_api, output_api):
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

  for f in input_api.AffectedFiles(False):
    if not 'test' in f.LocalPath() or not f.LocalPath().endswith('.cc'):
      continue

    # Search for MABYE_, DISABLE_ pairs.
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


def _CheckDCHECK_IS_ONHasBraces(input_api, output_api):
  """Checks to make sure DCHECK_IS_ON() does not skip the parentheses."""
  errors = []
  pattern = input_api.re.compile(r'DCHECK_IS_ON(?!\(\))',
                                 input_api.re.MULTILINE)
  for f in input_api.AffectedSourceFiles(input_api.FilterSourceFile):
    if (not f.LocalPath().endswith(('.cc', '.mm', '.h'))):
      continue
    for lnum, line in f.ChangedContents():
      if input_api.re.search(pattern, line):
        errors.append(output_api.PresubmitError(
          ('%s:%d: Use of DCHECK_IS_ON() must be written as "#if ' +
           'DCHECK_IS_ON()", not forgetting the parentheses.')
          % (f.LocalPath(), lnum)))
  return errors


def _FindHistogramNameInLine(histogram_name, line):
  """Tries to find a histogram name or prefix in a line."""
  if not "affected-histogram" in line:
    return histogram_name in line
  # A histogram_suffixes tag type has an affected-histogram name as a prefix of
  # the histogram_name.
  if not '"' in line:
    return False
  histogram_prefix = line.split('\"')[1]
  return histogram_prefix in histogram_name


def _CheckUmaHistogramChanges(input_api, output_api):
  """Check that UMA histogram names in touched lines can still be found in other
  lines of the patch or in histograms.xml. Note that this check would not catch
  the reverse: changes in histograms.xml not matched in the code itself."""
  touched_histograms = []
  histograms_xml_modifications = []
  call_pattern_c = r'\bUMA_HISTOGRAM.*\('
  call_pattern_java = r'\bRecordHistogram\.record[a-zA-Z]+Histogram\('
  name_pattern = r'"(.*?)"'
  single_line_c_re = input_api.re.compile(call_pattern_c + name_pattern)
  single_line_java_re = input_api.re.compile(call_pattern_java + name_pattern)
  split_line_c_prefix_re = input_api.re.compile(call_pattern_c)
  split_line_java_prefix_re = input_api.re.compile(call_pattern_java)
  split_line_suffix_re = input_api.re.compile(r'^\s*' + name_pattern)
  last_line_matched_prefix = False
  for f in input_api.AffectedFiles():
    # If histograms.xml itself is modified, keep the modified lines for later.
    if f.LocalPath().endswith(('histograms.xml')):
      histograms_xml_modifications = f.ChangedContents()
      continue
    if f.LocalPath().endswith(('cc', 'mm', 'cpp')):
      single_line_re = single_line_c_re
      split_line_prefix_re = split_line_c_prefix_re
    elif f.LocalPath().endswith(('java')):
      single_line_re = single_line_java_re
      split_line_prefix_re = split_line_java_prefix_re
    else:
      continue
    for line_num, line in f.ChangedContents():
      if last_line_matched_prefix:
        suffix_found = split_line_suffix_re.search(line)
        if suffix_found :
          touched_histograms.append([suffix_found.group(1), f, line_num])
          last_line_matched_prefix = False
          continue
      found = single_line_re.search(line)
      if found:
        touched_histograms.append([found.group(1), f, line_num])
        continue
      last_line_matched_prefix = split_line_prefix_re.search(line)

  # Search for the touched histogram names in the local modifications to
  # histograms.xml, and, if not found, on the base histograms.xml file.
  unmatched_histograms = []
  for histogram_info in touched_histograms:
    histogram_name_found = False
    for line_num, line in histograms_xml_modifications:
      histogram_name_found = _FindHistogramNameInLine(histogram_info[0], line)
      if histogram_name_found:
        break
    if not histogram_name_found:
      unmatched_histograms.append(histogram_info)

  histograms_xml_path = 'tools/metrics/histograms/histograms.xml'
  problems = []
  if unmatched_histograms:
    with open(histograms_xml_path) as histograms_xml:
      for histogram_name, f, line_num in unmatched_histograms:
        histograms_xml.seek(0)
        histogram_name_found = False
        for line in histograms_xml:
          histogram_name_found = _FindHistogramNameInLine(histogram_name, line)
          if histogram_name_found:
            break
        if not histogram_name_found:
          problems.append(' [%s:%d] %s' %
                          (f.LocalPath(), line_num, histogram_name))

  if not problems:
    return []
  return [output_api.PresubmitPromptWarning('Some UMA_HISTOGRAM lines have '
    'been modified and the associated histogram name has no match in either '
    '%s or the modifications of it:' % (histograms_xml_path),  problems)]


def _CheckFlakyTestUsage(input_api, output_api):
  """Check that FlakyTest annotation is our own instead of the android one"""
  pattern = input_api.re.compile(r'import android.test.FlakyTest;')
  files = []
  for f in input_api.AffectedSourceFiles(input_api.FilterSourceFile):
    if f.LocalPath().endswith('Test.java'):
      if pattern.search(input_api.ReadFile(f)):
        files.append(f)
  if len(files):
    return [output_api.PresubmitError(
      'Use org.chromium.base.test.util.FlakyTest instead of '
      'android.test.FlakyTest',
      files)]
  return []


def _CheckNoNewWStrings(input_api, output_api):
  """Checks to make sure we don't introduce use of wstrings."""
  problems = []
  for f in input_api.AffectedFiles():
    if (not f.LocalPath().endswith(('.cc', '.h')) or
        f.LocalPath().endswith(('test.cc', '_win.cc', '_win.h')) or
        '/win/' in f.LocalPath() or
        'chrome_elf' in f.LocalPath() or
        'install_static' in f.LocalPath()):
      continue

    allowWString = False
    for line_num, line in f.ChangedContents():
      if 'presubmit: allow wstring' in line:
        allowWString = True
      elif not allowWString and 'wstring' in line:
        problems.append('    %s:%d' % (f.LocalPath(), line_num))
        allowWString = False
      else:
        allowWString = False

  if not problems:
    return []
  return [output_api.PresubmitPromptWarning('New code should not use wstrings.'
      '  If you are calling a cross-platform API that accepts a wstring, '
      'fix the API.\n' +
      '\n'.join(problems))]


def _CheckNoDEPSGIT(input_api, output_api):
  """Make sure .DEPS.git is never modified manually."""
  if any(f.LocalPath().endswith('.DEPS.git') for f in
      input_api.AffectedFiles()):
    return [output_api.PresubmitError(
      'Never commit changes to .DEPS.git. This file is maintained by an\n'
      'automated system based on what\'s in DEPS and your changes will be\n'
      'overwritten.\n'
      'See https://sites.google.com/a/chromium.org/dev/developers/how-tos/'
      'get-the-code#Rolling_DEPS\n'
      'for more information')]
  return []


def _CheckValidHostsInDEPS(input_api, output_api):
  """Checks that DEPS file deps are from allowed_hosts."""
  # Run only if DEPS file has been modified to annoy fewer bystanders.
  if all(f.LocalPath() != 'DEPS' for f in input_api.AffectedFiles()):
    return []
  # Outsource work to gclient verify
  try:
    input_api.subprocess.check_output(['gclient', 'verify'],
                                      stderr=input_api.subprocess.STDOUT)
    return []
  except input_api.subprocess.CalledProcessError as error:
    return [output_api.PresubmitError(
        'DEPS file must have only git dependencies.',
        long_text=error.output)]


def _CheckNoBannedFunctions(input_api, output_api):
  """Make sure that banned functions are not used."""
  warnings = []
  errors = []

  def IsBlacklisted(affected_file, blacklist):
    local_path = affected_file.LocalPath()
    for item in blacklist:
      if input_api.re.match(item, local_path):
        return True
    return False

  def IsIosObcjFile(affected_file):
    local_path = affected_file.LocalPath()
    if input_api.os_path.splitext(local_path)[-1] not in ('.mm', '.m', '.h'):
      return False
    basename = input_api.os_path.basename(local_path)
    if 'ios' in basename.split('_'):
      return True
    for sep in (input_api.os_path.sep, input_api.os_path.altsep):
      if sep and 'ios' in local_path.split(sep):
        return True
    return False

  def CheckForMatch(affected_file, line_num, line, func_name, message, error):
    matched = False
    if func_name[0:1] == '/':
      regex = func_name[1:]
      if input_api.re.search(regex, line):
        matched = True
    elif func_name in line:
      matched = True
    if matched:
      problems = warnings
      if error:
        problems = errors
      problems.append('    %s:%d:' % (affected_file.LocalPath(), line_num))
      for message_line in message:
        problems.append('      %s' % message_line)

  file_filter = lambda f: f.LocalPath().endswith(('.java'))
  for f in input_api.AffectedFiles(file_filter=file_filter):
    for line_num, line in f.ChangedContents():
      for func_name, message, error in _BANNED_JAVA_FUNCTIONS:
        CheckForMatch(f, line_num, line, func_name, message, error)

  file_filter = lambda f: f.LocalPath().endswith(('.mm', '.m', '.h'))
  for f in input_api.AffectedFiles(file_filter=file_filter):
    for line_num, line in f.ChangedContents():
      for func_name, message, error in _BANNED_OBJC_FUNCTIONS:
        CheckForMatch(f, line_num, line, func_name, message, error)

  for f in input_api.AffectedFiles(file_filter=IsIosObcjFile):
    for line_num, line in f.ChangedContents():
      for func_name, message, error in _BANNED_IOS_OBJC_FUNCTIONS:
        CheckForMatch(f, line_num, line, func_name, message, error)

  file_filter = lambda f: f.LocalPath().endswith(('.cc', '.mm', '.h'))
  for f in input_api.AffectedFiles(file_filter=file_filter):
    for line_num, line in f.ChangedContents():
      for func_name, message, error, excluded_paths in _BANNED_CPP_FUNCTIONS:
        if IsBlacklisted(f, excluded_paths):
          continue
        CheckForMatch(f, line_num, line, func_name, message, error)

  result = []
  if (warnings):
    result.append(output_api.PresubmitPromptWarning(
        'Banned functions were used.\n' + '\n'.join(warnings)))
  if (errors):
    result.append(output_api.PresubmitError(
        'Banned functions were used.\n' + '\n'.join(errors)))
  return result


def _CheckNoPragmaOnce(input_api, output_api):
  """Make sure that banned functions are not used."""
  files = []
  pattern = input_api.re.compile(r'^#pragma\s+once',
                                 input_api.re.MULTILINE)
  for f in input_api.AffectedSourceFiles(input_api.FilterSourceFile):
    if not f.LocalPath().endswith('.h'):
      continue
    contents = input_api.ReadFile(f)
    if pattern.search(contents):
      files.append(f)

  if files:
    return [output_api.PresubmitError(
        'Do not use #pragma once in header files.\n'
        'See http://www.chromium.org/developers/coding-style#TOC-File-headers',
        files)]
  return []


def _CheckNoTrinaryTrueFalse(input_api, output_api):
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
  return [output_api.PresubmitPromptWarning(
      'Please consider avoiding the "? true : false" pattern if possible.\n' +
      '\n'.join(problems))]


def _CheckUnwantedDependencies(input_api, output_api):
  """Runs checkdeps on #include and import statements added in this
  change. Breaking - rules is an error, breaking ! rules is a
  warning.
  """
  import sys
  # We need to wait until we have an input_api object and use this
  # roundabout construct to import checkdeps because this file is
  # eval-ed and thus doesn't have __file__.
  original_sys_path = sys.path
  try:
    sys.path = sys.path + [input_api.os_path.join(
        input_api.PresubmitLocalPath(), 'buildtools', 'checkdeps')]
    import checkdeps
    from cpp_checker import CppChecker
    from java_checker import JavaChecker
    from proto_checker import ProtoChecker
    from rules import Rule
  finally:
    # Restore sys.path to what it was before.
    sys.path = original_sys_path

  added_includes = []
  added_imports = []
  added_java_imports = []
  for f in input_api.AffectedFiles():
    if CppChecker.IsCppFile(f.LocalPath()):
      changed_lines = [line for _, line in f.ChangedContents()]
      added_includes.append([f.AbsoluteLocalPath(), changed_lines])
    elif ProtoChecker.IsProtoFile(f.LocalPath()):
      changed_lines = [line for _, line in f.ChangedContents()]
      added_imports.append([f.AbsoluteLocalPath(), changed_lines])
    elif JavaChecker.IsJavaFile(f.LocalPath()):
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
    results.append(output_api.PresubmitError(
        'You added one or more %s that violate checkdeps rules.'
            % " and ".join(error_subjects),
        error_descriptions))
  if warning_descriptions:
    results.append(output_api.PresubmitPromptOrNotify(
        'You added one or more %s of files that are temporarily\n'
        'allowed but being removed. Can you avoid introducing the\n'
        '%s? See relevant DEPS file(s) for details and contacts.' %
            (" and ".join(warning_subjects), "/".join(warning_subjects)),
        warning_descriptions))
  return results


def _CheckFilePermissions(input_api, output_api):
  """Check that all files have their permissions properly set."""
  if input_api.platform == 'win32':
    return []
  checkperms_tool = input_api.os_path.join(
      input_api.PresubmitLocalPath(),
      'tools', 'checkperms', 'checkperms.py')
  args = [input_api.python_executable, checkperms_tool,
          '--root', input_api.change.RepositoryRoot()]
  with input_api.CreateTemporaryFile() as file_list:
    for f in input_api.AffectedFiles():
      # checkperms.py file/directory arguments must be relative to the
      # repository.
      file_list.write(f.LocalPath() + '\n')
    file_list.close()
    args += ['--file-list', file_list.name]
    try:
      input_api.subprocess.check_output(args)
      return []
    except input_api.subprocess.CalledProcessError as error:
      return [output_api.PresubmitError(
          'checkperms.py failed:',
          long_text=error.output)]


def _CheckTeamTags(input_api, output_api):
  """Checks that OWNERS files have consistent TEAM and COMPONENT tags."""
  checkteamtags_tool = input_api.os_path.join(
      input_api.PresubmitLocalPath(),
      'tools', 'checkteamtags', 'checkteamtags.py')
  args = [input_api.python_executable, checkteamtags_tool,
          '--root', input_api.change.RepositoryRoot()]
  files = [f.LocalPath() for f in input_api.AffectedFiles(include_deletes=False)
           if input_api.os_path.basename(f.AbsoluteLocalPath()).upper() ==
           'OWNERS']
  try:
    if files:
      input_api.subprocess.check_output(args + files)
    return []
  except input_api.subprocess.CalledProcessError as error:
    return [output_api.PresubmitError(
        'checkteamtags.py failed:',
        long_text=error.output)]


def _CheckNoAuraWindowPropertyHInHeaders(input_api, output_api):
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
    results.append(output_api.PresubmitError(
      'Header files should not include ui/aura/window_property.h', errors))
  return results


def _CheckForVersionControlConflictsInFile(input_api, f):
  pattern = input_api.re.compile('^(?:<<<<<<<|>>>>>>>) |^=======$')
  errors = []
  for line_num, line in f.ChangedContents():
    if f.LocalPath().endswith('.md'):
      # First-level headers in markdown look a lot like version control
      # conflict markers. http://daringfireball.net/projects/markdown/basics
      continue
    if pattern.match(line):
      errors.append('    %s:%d %s' % (f.LocalPath(), line_num, line))
  return errors


def _CheckForVersionControlConflicts(input_api, output_api):
  """Usually this is not intentional and will cause a compile failure."""
  errors = []
  for f in input_api.AffectedFiles():
    errors.extend(_CheckForVersionControlConflictsInFile(input_api, f))

  results = []
  if errors:
    results.append(output_api.PresubmitError(
      'Version control conflict markers found, please resolve.', errors))
  return results


def _CheckGoogleSupportAnswerUrl(input_api, output_api):
  pattern = input_api.re.compile('support\.google\.com\/chrome.*/answer')
  errors = []
  for f in input_api.AffectedFiles():
    for line_num, line in f.ChangedContents():
      if pattern.search(line):
        errors.append('    %s:%d %s' % (f.LocalPath(), line_num, line))

  results = []
  if errors:
    results.append(output_api.PresubmitPromptWarning(
      'Found Google support URL addressed by answer number. Please replace '
      'with a p= identifier instead. See crbug.com/679462\n', errors))
  return results


def _CheckHardcodedGoogleHostsInLowerLayers(input_api, output_api):
  def FilterFile(affected_file):
    """Filter function for use with input_api.AffectedSourceFiles,
    below.  This filters out everything except non-test files from
    top-level directories that generally speaking should not hard-code
    service URLs (e.g. src/android_webview/, src/content/ and others).
    """
    return input_api.FilterSourceFile(
      affected_file,
      white_list=[r'^(android_webview|base|content|net)[\\/].*'],
      black_list=(_EXCLUDED_PATHS +
                  _TEST_CODE_EXCLUDED_PATHS +
                  input_api.DEFAULT_BLACK_LIST))

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
    return [output_api.PresubmitPromptOrNotify(
        'Most layers below src/chrome/ should not hardcode service URLs.\n'
        'Are you sure this is correct?',
        ['  %s:%d:  %s' % (
            problem[0], problem[1], problem[2]) for problem in problems])]
  else:
    return []


# TODO: add unit tests.
def _CheckNoAbbreviationInPngFileName(input_api, output_api):
  """Makes sure there are no abbreviations in the name of PNG files.
  The native_client_sdk directory is excluded because it has auto-generated PNG
  files for documentation.
  """
  errors = []
  white_list = [r'.*_[a-z]_.*\.png$|.*_[a-z]\.png$']
  black_list = [r'^native_client_sdk[\\/]']
  file_filter = lambda f: input_api.FilterSourceFile(
      f, white_list=white_list, black_list=black_list)
  for f in input_api.AffectedFiles(include_deletes=False,
                                   file_filter=file_filter):
    errors.append('    %s' % f.LocalPath())

  results = []
  if errors:
    results.append(output_api.PresubmitError(
        'The name of PNG files should not have abbreviations. \n'
        'Use _hover.png, _center.png, instead of _h.png, _c.png.\n'
        'Contact oshima@chromium.org if you have questions.', errors))
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
  for _, rules in parsed_deps.get('specific_include_rules',
                                              {}).iteritems():
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
  }
  exec contents in global_scope, local_scope
  return local_scope


def _CalculateAddedDeps(os_path, old_contents, new_contents):
  """Helper method for _CheckAddedDepsHaveTargetApprovals. Returns
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


def _CheckAddedDepsHaveTargetApprovals(input_api, output_api):
  """When a dependency prefixed with + is added to a DEPS file, we
  want to make sure that the change is reviewed by an OWNER of the
  target file or directory, to avoid layering violations from being
  introduced. This check verifies that this happens.
  """
  virtual_depended_on_files = set()

  file_filter = lambda f: not input_api.re.match(
      r"^third_party[\\/](WebKit|blink)[\\/].*", f.LocalPath())
  for f in input_api.AffectedFiles(include_deletes=False,
                                   file_filter=file_filter):
    filename = input_api.os_path.basename(f.LocalPath())
    if filename == 'DEPS':
      virtual_depended_on_files.update(_CalculateAddedDeps(
          input_api.os_path,
          '\n'.join(f.OldContents()),
          '\n'.join(f.NewContents())))

  if not virtual_depended_on_files:
    return []

  if input_api.is_committing:
    if input_api.tbr:
      return [output_api.PresubmitNotifyResult(
          '--tbr was specified, skipping OWNERS check for DEPS additions')]
    if input_api.dry_run:
      return [output_api.PresubmitNotifyResult(
          'This is a dry run, skipping OWNERS check for DEPS additions')]
    if not input_api.change.issue:
      return [output_api.PresubmitError(
          "DEPS approval by OWNERS check failed: this change has "
          "no change number, so we can't check it for approvals.")]
    output = output_api.PresubmitError
  else:
    output = output_api.PresubmitNotifyResult

  owners_db = input_api.owners_db
  owner_email, reviewers = (
      input_api.canned_checks.GetCodereviewOwnerAndReviewers(
        input_api,
        owners_db.email_regexp,
        approval_needed=input_api.is_committing))

  owner_email = owner_email or input_api.change.author_email

  reviewers_plus_owner = set(reviewers)
  if owner_email:
    reviewers_plus_owner.add(owner_email)
  missing_files = owners_db.files_not_covered_by(virtual_depended_on_files,
                                                 reviewers_plus_owner)

  # We strip the /DEPS part that was added by
  # _FilesToCheckForIncomingDeps to fake a path to a file in a
  # directory.
  def StripDeps(path):
    start_deps = path.rfind('/DEPS')
    if start_deps != -1:
      return path[:start_deps]
    else:
      return path
  unapproved_dependencies = ["'+%s'," % StripDeps(path)
                             for path in missing_files]

  if unapproved_dependencies:
    output_list = [
      output('You need LGTM from owners of depends-on paths in DEPS that were '
             'modified in this CL:\n    %s' %
                 '\n    '.join(sorted(unapproved_dependencies)))]
    suggested_owners = owners_db.reviewers_for(missing_files, owner_email)
    output_list.append(output(
        'Suggested missing target path OWNERS:\n    %s' %
            '\n    '.join(suggested_owners or [])))
    return output_list

  return []


# TODO: add unit tests.
def _CheckSpamLogging(input_api, output_api):
  file_inclusion_pattern = [r'.+%s' % _IMPLEMENTATION_EXTENSIONS]
  black_list = (_EXCLUDED_PATHS +
                _TEST_CODE_EXCLUDED_PATHS +
                input_api.DEFAULT_BLACK_LIST +
                (r"^base[\\/]logging\.h$",
                 r"^base[\\/]logging\.cc$",
                 r"^chrome[\\/]app[\\/]chrome_main_delegate\.cc$",
                 r"^chrome[\\/]browser[\\/]chrome_browser_main\.cc$",
                 r"^chrome[\\/]browser[\\/]ui[\\/]startup[\\/]"
                     r"startup_browser_creator\.cc$",
                 r"^chrome[\\/]installer[\\/]setup[\\/].*",
                 r"^chrome[\\/]chrome_cleaner[\\/].*",
                 r"chrome[\\/]browser[\\/]diagnostics[\\/]" +
                     r"diagnostics_writer\.cc$",
                 r"^chrome_elf[\\/]dll_hash[\\/]dll_hash_main\.cc$",
                 r"^chromecast[\\/]",
                 r"^cloud_print[\\/]",
                 r"^components[\\/]browser_watcher[\\/]"
                     r"dump_stability_report_main_win.cc$",
                 r"^components[\\/]html_viewer[\\/]"
                     r"web_test_delegate_impl\.cc$",
                 r"^components[\\/]zucchini[\\/].*",
                 # TODO(peter): Remove this exception. https://crbug.com/534537
                 r"^content[\\/]browser[\\/]notifications[\\/]"
                     r"notification_event_dispatcher_impl\.cc$",
                 r"^content[\\/]common[\\/]gpu[\\/]client[\\/]"
                     r"gl_helper_benchmark\.cc$",
                 r"^courgette[\\/]courgette_minimal_tool\.cc$",
                 r"^courgette[\\/]courgette_tool\.cc$",
                 r"^extensions[\\/]renderer[\\/]logging_native_handler\.cc$",
                 r"^ipc[\\/]ipc_logging\.cc$",
                 r"^native_client_sdk[\\/]",
                 r"^remoting[\\/]base[\\/]logging\.h$",
                 r"^remoting[\\/]host[\\/].*",
                 r"^sandbox[\\/]linux[\\/].*",
                 r"^tools[\\/]",
                 r"^ui[\\/]base[\\/]resource[\\/]data_pack.cc$",
                 r"^ui[\\/]aura[\\/]bench[\\/]bench_main\.cc$",
                 r"^ui[\\/]ozone[\\/]platform[\\/]cast[\\/]",
                 r"^storage[\\/]browser[\\/]fileapi[\\/]" +
                     r"dump_file_system.cc$",
                 r"^headless[\\/]app[\\/]headless_shell\.cc$"))
  source_file_filter = lambda x: input_api.FilterSourceFile(
      x, white_list=file_inclusion_pattern, black_list=black_list)

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
    return [output_api.PresubmitError(
      'These files spam the console log with LOG(INFO):',
      items=log_info)]
  if printf:
    return [output_api.PresubmitError(
      'These files spam the console log with printf/fprintf:',
      items=printf)]
  return []


def _CheckForAnonymousVariables(input_api, output_api):
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
    return [output_api.PresubmitError(
      'These lines create anonymous variables that need to be named:',
      items=errors)]
  return []


def _CheckUniquePtr(input_api, output_api):
  # Returns whether |template_str| is of the form <T, U...> for some types T
  # and U. Assumes that |template_str| is already in the form <...>.
  def HasMoreThanOneArg(template_str):
    # Level of <...> nesting.
    nesting = 0
    for c in template_str:
      if c == '<':
        nesting += 1
      elif c == '>':
        nesting -= 1
      elif c == ',' and nesting == 1:
        return True
    return False

  file_inclusion_pattern = [r'.+%s' % _IMPLEMENTATION_EXTENSIONS]
  sources = lambda affected_file: input_api.FilterSourceFile(
      affected_file,
      black_list=(_EXCLUDED_PATHS + _TEST_CODE_EXCLUDED_PATHS +
                  input_api.DEFAULT_BLACK_LIST),
      white_list=file_inclusion_pattern)

  # Pattern to capture a single "<...>" block of template arguments. It can
  # handle linearly nested blocks, such as "<std::vector<std::set<T>>>", but
  # cannot handle branching structures, such as "<pair<set<T>,set<U>>". The
  # latter would likely require counting that < and > match, which is not
  # expressible in regular languages. Should the need arise, one can introduce
  # limited counting (matching up to a total number of nesting depth), which
  # should cover all practical cases for already a low nesting limit.
  template_arg_pattern = (
      r'<[^>]*'       # Opening block of <.
      r'>([^<]*>)?')  # Closing block of >.
  # Prefix expressing that whatever follows is not already inside a <...>
  # block.
  not_inside_template_arg_pattern = r'(^|[^<,\s]\s*)'
  null_construct_pattern = input_api.re.compile(
      not_inside_template_arg_pattern
      + r'\bstd::unique_ptr'
      + template_arg_pattern
      + r'\(\)')

  # Same as template_arg_pattern, but excluding type arrays, e.g., <T[]>.
  template_arg_no_array_pattern = (
      r'<[^>]*[^]]'        # Opening block of <.
      r'>([^(<]*[^]]>)?')  # Closing block of >.
  # Prefix saying that what follows is the start of an expression.
  start_of_expr_pattern = r'(=|\breturn|^)\s*'
  # Suffix saying that what follows are call parentheses with a non-empty list
  # of arguments.
  nonempty_arg_list_pattern = r'\(([^)]|$)'
  # Put the template argument into a capture group for deeper examination later.
  return_construct_pattern = input_api.re.compile(
      start_of_expr_pattern
      + r'std::unique_ptr'
      + '(?P<template_arg>'
      + template_arg_no_array_pattern
      + ')'
      + nonempty_arg_list_pattern)

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
      if return_construct_result and not HasMoreThanOneArg(
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
    errors.append(output_api.PresubmitError(
        'The following files use std::unique_ptr<T>(). Use nullptr instead.',
        problems_nullptr))
  if problems_constructor:
    errors.append(output_api.PresubmitError(
        'The following files use explicit std::unique_ptr constructor.'
        'Use std::make_unique<T>() instead.',
        problems_constructor))
  return errors


def _CheckUserActionUpdate(input_api, output_api):
  """Checks if any new user action has been added."""
  if any('actions.xml' == input_api.os_path.basename(f) for f in
         input_api.LocalPaths()):
    # If actions.xml is already included in the changelist, the PRESUBMIT
    # for actions.xml will do a more complete presubmit check.
    return []

  file_filter = lambda f: f.LocalPath().endswith(('.cc', '.mm'))
  action_re = r'[^a-zA-Z]UserMetricsAction\("([^"]*)'
  current_actions = None
  for f in input_api.AffectedFiles(file_filter=file_filter):
    for line_num, line in f.ChangedContents():
      match = input_api.re.search(action_re, line)
      if match:
        # Loads contents in tools/metrics/actions/actions.xml to memory. It's
        # loaded only once.
        if not current_actions:
          with open('tools/metrics/actions/actions.xml') as actions_f:
            current_actions = actions_f.read()
        # Search for the matched user action name in |current_actions|.
        for action_name in match.groups():
          action = 'name="{0}"'.format(action_name)
          if action not in current_actions:
            return [output_api.PresubmitPromptWarning(
              'File %s line %d: %s is missing in '
              'tools/metrics/actions/actions.xml. Please run '
              'tools/metrics/actions/extract_actions.py to update.'
              % (f.LocalPath(), line_num, action_name))]
  return []


def _ImportJSONCommentEater(input_api):
  import sys
  sys.path = sys.path + [input_api.os_path.join(
      input_api.PresubmitLocalPath(),
      'tools', 'json_comment_eater')]
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
    idl_schema = input_api.os_path.join(
        input_api.PresubmitLocalPath(),
        'tools', 'json_schema_compiler', 'idl_schema.py')
    process = input_api.subprocess.Popen(
        [input_api.python_executable, idl_schema],
        stdin=input_api.subprocess.PIPE,
        stdout=input_api.subprocess.PIPE,
        stderr=input_api.subprocess.PIPE,
        universal_newlines=True)
    (_, error) = process.communicate(input=contents)
    return error or None
  except ValueError as e:
    return e


def _CheckParseErrors(input_api, output_api):
  """Check that IDL and JSON files do not contain syntax errors."""
  actions = {
    '.idl': _GetIDLParseError,
    '.json': _GetJSONParseError,
  }
  # Most JSON files are preprocessed and support comments, but these do not.
  json_no_comments_patterns = [
    r'^testing[\\/]',
  ]
  # Only run IDL checker on files in these directories.
  idl_included_patterns = [
    r'^chrome[\\/]common[\\/]extensions[\\/]api[\\/]',
    r'^extensions[\\/]common[\\/]api[\\/]',
  ]

  def get_action(affected_file):
    filename = affected_file.LocalPath()
    return actions.get(input_api.os_path.splitext(filename)[1])

  def FilterFile(affected_file):
    action = get_action(affected_file)
    if not action:
      return False
    path = affected_file.LocalPath()

    if _MatchesFile(input_api, _KNOWN_INVALID_JSON_FILE_PATTERNS, path):
      return False

    if (action == _GetIDLParseError and
        not _MatchesFile(input_api, idl_included_patterns, path)):
      return False
    return True

  results = []
  for affected_file in input_api.AffectedFiles(
      file_filter=FilterFile, include_deletes=False):
    action = get_action(affected_file)
    kwargs = {}
    if (action == _GetJSONParseError and
        _MatchesFile(input_api, json_no_comments_patterns,
                     affected_file.LocalPath())):
      kwargs['eat_comments'] = False
    parse_error = action(input_api,
                         affected_file.AbsoluteLocalPath(),
                         **kwargs)
    if parse_error:
      results.append(output_api.PresubmitError('%s could not be parsed: %s' %
          (affected_file.LocalPath(), parse_error)))
  return results


def _CheckJavaStyle(input_api, output_api):
  """Runs checkstyle on changed java files and returns errors if any exist."""
  import sys
  original_sys_path = sys.path
  try:
    sys.path = sys.path + [input_api.os_path.join(
        input_api.PresubmitLocalPath(), 'tools', 'android', 'checkstyle')]
    import checkstyle
  finally:
    # Restore sys.path to what it was before.
    sys.path = original_sys_path

  return checkstyle.RunCheckstyle(
      input_api, output_api, 'tools/android/checkstyle/chromium-style-5.0.xml',
      black_list=_EXCLUDED_PATHS + input_api.DEFAULT_BLACK_LIST)


def _MatchesFile(input_api, patterns, path):
  for pattern in patterns:
    if input_api.re.search(pattern, path):
      return True
  return False


def _GetOwnersFilesToCheckForIpcOwners(input_api):
  """Gets a list of OWNERS files to check for correct security owners.

  Returns:
    A dictionary mapping an OWNER file to the list of OWNERS rules it must
    contain to cover IPC-related files with noparent reviewer rules.
  """
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
      '*_struct_traits*.*',
      '*_type_converter*.*',
      '*.typemap',
      # Android native IPC:
      '*.aidl',
      # Blink uses a different file naming convention:
      '*EnumTraits*.*',
      "*MojomTraits*.*",
      '*StructTraits*.*',
      '*TypeConverter*.*',
  ]

  # These third_party directories do not contain IPCs, but contain files
  # matching the above patterns, which trigger false positives.
  exclude_paths = [
      'third_party/crashpad/*',
      'third_party/protobuf/benchmarks/python/*',
      'third_party/third_party/blink/renderer/platform/bindings/*',
      'third_party/win_build_output/*',
  ]

  # Dictionary mapping an OWNERS file path to Patterns.
  # Patterns is a dictionary mapping glob patterns (suitable for use in per-file
  # rules ) to a PatternEntry.
  # PatternEntry is a dictionary with two keys:
  # - 'files': the files that are matched by this pattern
  # - 'rules': the per-file rules needed for this pattern
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

  def AddPatternToCheck(input_file, pattern):
    owners_file = input_api.os_path.join(
        input_api.os_path.dirname(input_file.LocalPath()), 'OWNERS')
    if owners_file not in to_check:
      to_check[owners_file] = {}
    if pattern not in to_check[owners_file]:
      to_check[owners_file][pattern] = {
          'files': [],
          'rules': [
              'per-file %s=set noparent' % pattern,
              'per-file %s=file://ipc/SECURITY_OWNERS' % pattern,
          ]
      }
    to_check[owners_file][pattern]['files'].append(input_file)

  # Iterate through the affected files to see what we actually need to check
  # for. We should only nag patch authors about per-file rules if a file in that
  # directory would match that pattern. If a directory only contains *.mojom
  # files and no *_messages*.h files, we should only nag about rules for
  # *.mojom files.
  for f in input_api.AffectedFiles(include_deletes=False):
    # Manifest files don't have a strong naming convention. Instead, scan
    # affected files for .json files and see if they look like a manifest.
    if (f.LocalPath().endswith('.json') and
        not _MatchesFile(input_api, _KNOWN_INVALID_JSON_FILE_PATTERNS,
                         f.LocalPath())):
      json_comment_eater = _ImportJSONCommentEater(input_api)
      mostly_json_lines = '\n'.join(f.NewContents())
      # Comments aren't allowed in strict JSON, so filter them out.
      json_lines = json_comment_eater.Nom(mostly_json_lines)
      try:
        json_content = input_api.json.loads(json_lines)
      except:
        # There's another PRESUBMIT check that already verifies that JSON files
        # are not invalid, so no need to emit another warning here.
        continue
      if 'interface_provider_specs' in json_content:
        AddPatternToCheck(f, input_api.os_path.basename(f.LocalPath()))
    for pattern in file_patterns:
      if input_api.fnmatch.fnmatch(
          input_api.os_path.basename(f.LocalPath()), pattern):
        skip = False
        for exclude in exclude_paths:
          if input_api.fnmatch.fnmatch(f.LocalPath(), exclude):
            skip = True
            break
        if skip:
          continue
        AddPatternToCheck(f, pattern)
        break

  return to_check


def _CheckIpcOwners(input_api, output_api):
  """Checks that affected files involving IPC have an IPC OWNERS rule."""
  to_check = _GetOwnersFilesToCheckForIpcOwners(input_api)

  if to_check:
    # If there are any OWNERS files to check, there are IPC-related changes in
    # this CL. Auto-CC the review list.
    output_api.AppendCC('ipc-security-reviews@chromium.org')

  # Go through the OWNERS files to check, filtering out rules that are already
  # present in that OWNERS file.
  for owners_file, patterns in to_check.iteritems():
    try:
      with file(owners_file) as f:
        lines = set(f.read().splitlines())
        for entry in patterns.itervalues():
          entry['rules'] = [rule for rule in entry['rules'] if rule not in lines
                           ]
    except IOError:
      # No OWNERS file, so all the rules are definitely missing.
      continue

  # All the remaining lines weren't found in OWNERS files, so emit an error.
  errors = []
  for owners_file, patterns in to_check.iteritems():
    missing_lines = []
    files = []
    for _, entry in patterns.iteritems():
      missing_lines.extend(entry['rules'])
      files.extend(['  %s' % f.LocalPath() for f in entry['files']])
    if missing_lines:
      errors.append(
          'Because of the presence of files:\n%s\n\n'
          '%s needs the following %d lines added:\n\n%s' %
          ('\n'.join(files), owners_file, len(missing_lines),
           '\n'.join(missing_lines)))

  results = []
  if errors:
    if input_api.is_committing:
      output = output_api.PresubmitError
    else:
      output = output_api.PresubmitPromptWarning
    results.append(output(
        'Found OWNERS files that need to be updated for IPC security ' +
        'review coverage.\nPlease update the OWNERS files below:',
        long_text='\n\n'.join(errors)))

  return results


def _CheckUselessForwardDeclarations(input_api, output_api):
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
    if (f.LocalPath().startswith('third_party') and
        not f.LocalPath().startswith('third_party/blink') and
        not f.LocalPath().startswith('third_party\\blink') and
        not f.LocalPath().startswith('third_party/WebKit') and
        not f.LocalPath().startswith('third_party\\WebKit')):
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
      if (line.startswith('-') and not line.startswith('--') or
          line.startswith('+') and not line.startswith('++')):
        for decl in useless_fwd_decls:
          if input_api.re.search(r'\b%s\b' % decl, line[1:]):
            results.append(output_api.PresubmitPromptWarning(
              '%s: %s forward declaration is no longer needed' %
              (f.LocalPath(), decl)))
            useless_fwd_decls.remove(decl)

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
      black_list=(_EXCLUDED_PATHS +
                  _TEST_CODE_EXCLUDED_PATHS +
                  input_api.DEFAULT_BLACK_LIST +
                  (r'^chromecast[\\/].*',
                   r'^remoting[\\/].*')),
      white_list=[r'.*\.java$'])

  for f in input_api.AffectedSourceFiles(sources):
    for line_num, line in f.ChangedContents():
      if toast_import_pattern.search(line):
        errors.append("%s:%d" % (f.LocalPath(), line_num))

  results = []

  if errors:
    results.append(output_api.PresubmitError(
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
    r"^chrome[\\/]android[\\/]webapk[\\/].*",
    # WebView license viewer code cannot depend on //base; used in stub APK.
    r"^android_webview[\\/]glue[\\/]java[\\/]src[\\/]com[\\/]android[\\/]"
    r"webview[\\/]chromium[\\/]License.*",
    # The customtabs_benchmark is a small app that does not depend on Chromium
    # java pieces.
    r"tools[\\/]android[\\/]customtabs_benchmark[\\/].*",
  ]

  cr_log_import_pattern = input_api.re.compile(
      r'^import org\.chromium\.base\.Log;$', input_api.re.MULTILINE)
  class_in_base_pattern = input_api.re.compile(
      r'^package org\.chromium\.base;$', input_api.re.MULTILINE)
  has_some_log_import_pattern = input_api.re.compile(
      r'^import .*\.Log;$', input_api.re.MULTILINE)
  # Extract the tag from lines like `Log.d(TAG, "*");` or `Log.d("TAG", "*");`
  log_call_pattern = input_api.re.compile(r'^\s*Log\.\w\((?P<tag>\"?\w+\"?)\,')
  log_decl_pattern = input_api.re.compile(
      r'^\s*private static final String TAG = "(?P<name>(.*))";',
      input_api.re.MULTILINE)

  REF_MSG = ('See docs/android_logging.md '
            'or contact dgn@chromium.org for more info.')
  sources = lambda x: input_api.FilterSourceFile(x, white_list=[r'.*\.java$'],
      black_list=cr_log_check_excluded_paths)

  tag_decl_errors = []
  tag_length_errors = []
  tag_errors = []
  tag_with_dot_errors = []
  util_log_errors = []

  for f in input_api.AffectedSourceFiles(sources):
    file_content = input_api.ReadFile(f)
    has_modified_logs = False

    # Per line checks
    if (cr_log_import_pattern.search(file_content) or
        (class_in_base_pattern.search(file_content) and
            not has_some_log_import_pattern.search(file_content))):
      # Checks to run for files using cr log
      for line_num, line in f.ChangedContents():

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
    results.append(output_api.PresubmitPromptWarning(
        'Please define your tags using the suggested format: .\n'
        '"private static final String TAG = "<package tag>".\n'
        'They will be prepended with "cr_" automatically.\n' + REF_MSG,
        tag_decl_errors))

  if tag_length_errors:
    results.append(output_api.PresubmitError(
        'The tag length is restricted by the system to be at most '
        '20 characters.\n' + REF_MSG,
        tag_length_errors))

  if tag_errors:
    results.append(output_api.PresubmitPromptWarning(
        'Please use a variable named "TAG" for your log tags.\n' + REF_MSG,
        tag_errors))

  if util_log_errors:
    results.append(output_api.PresubmitPromptWarning(
        'Please use org.chromium.base.Log for new logs.\n' + REF_MSG,
        util_log_errors))

  if tag_with_dot_errors:
    results.append(output_api.PresubmitPromptWarning(
        'Dot in log tags cause them to be elided in crash reports.\n' + REF_MSG,
        tag_with_dot_errors))

  return results


def _CheckAndroidTestJUnitFrameworkImport(input_api, output_api):
  """Checks that junit.framework.* is no longer used."""
  deprecated_junit_framework_pattern = input_api.re.compile(
      r'^import junit\.framework\..*;',
      input_api.re.MULTILINE)
  sources = lambda x: input_api.FilterSourceFile(
      x, white_list=[r'.*\.java$'], black_list=None)
  errors = []
  for f in input_api.AffectedFiles(sources):
    for line_num, line in f.ChangedContents():
      if deprecated_junit_framework_pattern.search(line):
        errors.append("%s:%d" % (f.LocalPath(), line_num))

  results = []
  if errors:
    results.append(output_api.PresubmitError(
      'APIs from junit.framework.* are deprecated, please use JUnit4 framework'
      '(org.junit.*) from //third_party/junit. Contact yolandyan@chromium.org'
      ' if you have any question.', errors))
  return results


def _CheckAndroidTestJUnitInheritance(input_api, output_api):
  """Checks that if new Java test classes have inheritance.
     Either the new test class is JUnit3 test or it is a JUnit4 test class
     with a base class, either case is undesirable.
  """
  class_declaration_pattern = input_api.re.compile(r'^public class \w*Test ')

  sources = lambda x: input_api.FilterSourceFile(
      x, white_list=[r'.*Test\.java$'], black_list=None)
  errors = []
  for f in input_api.AffectedFiles(sources):
    if not f.OldContents():
      class_declaration_start_flag = False
      for line_num, line in f.ChangedContents():
        if class_declaration_pattern.search(line):
          class_declaration_start_flag = True
        if class_declaration_start_flag and ' extends ' in line:
          errors.append('%s:%d' % (f.LocalPath(), line_num))
        if '{' in line:
          class_declaration_start_flag = False

  results = []
  if errors:
    results.append(output_api.PresubmitPromptWarning(
      'The newly created files include Test classes that inherits from base'
      ' class. Please do not use inheritance in JUnit4 tests or add new'
      ' JUnit3 tests. Contact yolandyan@chromium.org if you have any'
      ' questions.', errors))
  return results


def _CheckAndroidTestAnnotationUsage(input_api, output_api):
  """Checks that android.test.suitebuilder.annotation.* is no longer used."""
  deprecated_annotation_import_pattern = input_api.re.compile(
      r'^import android\.test\.suitebuilder\.annotation\..*;',
      input_api.re.MULTILINE)
  sources = lambda x: input_api.FilterSourceFile(
      x, white_list=[r'.*\.java$'], black_list=None)
  errors = []
  for f in input_api.AffectedFiles(sources):
    for line_num, line in f.ChangedContents():
      if deprecated_annotation_import_pattern.search(line):
        errors.append("%s:%d" % (f.LocalPath(), line_num))

  results = []
  if errors:
    results.append(output_api.PresubmitError(
      'Annotations in android.test.suitebuilder.annotation have been'
      ' deprecated since API level 24. Please use android.support.test.filters'
      ' from //third_party/android_support_test_runner:runner_java instead.'
      ' Contact yolandyan@chromium.org if you have any questions.', errors))
  return results


def _CheckAndroidNewMdpiAssetLocation(input_api, output_api):
  """Checks if MDPI assets are placed in a correct directory."""
  file_filter = lambda f: (f.LocalPath().endswith('.png') and
                           ('/res/drawable/' in f.LocalPath() or
                            '/res/drawable-ldrtl/' in f.LocalPath()))
  errors = []
  for f in input_api.AffectedFiles(include_deletes=False,
                                   file_filter=file_filter):
    errors.append('    %s' % f.LocalPath())

  results = []
  if errors:
    results.append(output_api.PresubmitError(
        'MDPI assets should be placed in /res/drawable-mdpi/ or '
        '/res/drawable-ldrtl-mdpi/\ninstead of /res/drawable/ and'
        '/res/drawable-ldrtl/.\n'
        'Contact newt@chromium.org if you have questions.', errors))
  return results


def _CheckAndroidWebkitImports(input_api, output_api):
  """Checks that code uses org.chromium.base.Callback instead of
     android.widget.ValueCallback except in the WebView glue layer.
  """
  valuecallback_import_pattern = input_api.re.compile(
      r'^import android\.webkit\.ValueCallback;$')

  errors = []

  sources = lambda affected_file: input_api.FilterSourceFile(
      affected_file,
      black_list=(_EXCLUDED_PATHS +
                  _TEST_CODE_EXCLUDED_PATHS +
                  input_api.DEFAULT_BLACK_LIST +
                  (r'^android_webview[\\/]glue[\\/].*',)),
      white_list=[r'.*\.java$'])

  for f in input_api.AffectedSourceFiles(sources):
    for line_num, line in f.ChangedContents():
      if valuecallback_import_pattern.search(line):
        errors.append("%s:%d" % (f.LocalPath(), line_num))

  results = []

  if errors:
    results.append(output_api.PresubmitError(
        'android.webkit.ValueCallback usage is detected outside of the glue'
        ' layer. To stay compatible with the support library, android.webkit.*'
        ' classes should only be used inside the glue layer and'
        ' org.chromium.base.Callback should be used instead.',
        errors))

  return results


class PydepsChecker(object):
  def __init__(self, input_api, pydeps_files):
    self._file_cache = {}
    self._input_api = input_api
    self._pydeps_files = pydeps_files

  def _LoadFile(self, path):
    """Returns the list of paths within a .pydeps file relative to //."""
    if path not in self._file_cache:
      with open(path) as f:
        self._file_cache[path] = f.read()
    return self._file_cache[path]

  def _ComputeNormalizedPydepsEntries(self, pydeps_path):
    """Returns an interable of paths within the .pydep, relativized to //."""
    os_path = self._input_api.os_path
    pydeps_dir = os_path.dirname(pydeps_path)
    entries = (l.rstrip() for l in self._LoadFile(pydeps_path).splitlines()
               if not l.startswith('*'))
    return (os_path.normpath(os_path.join(pydeps_dir, e)) for e in entries)

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
      if local_path  == 'DEPS':
        return self._pydeps_files
      elif local_path.endswith('.pydeps'):
        if local_path in self._pydeps_files:
          affected_pydeps.add(local_path)
      elif local_path.endswith('.py'):
        if file_to_pydeps_map is None:
          file_to_pydeps_map = self._CreateFilesToPydepsMap()
        affected_pydeps.update(file_to_pydeps_map.get(local_path, ()))
    return affected_pydeps

  def DetermineIfStale(self, pydeps_path):
    """Runs print_python_deps.py to see if the files is stale."""
    import difflib
    import os

    old_pydeps_data = self._LoadFile(pydeps_path).splitlines()
    cmd = old_pydeps_data[1][1:].strip()
    env = dict(os.environ)
    env['PYTHONDONTWRITEBYTECODE'] = '1'
    new_pydeps_data = self._input_api.subprocess.check_output(
        cmd + ' --output ""', shell=True, env=env)
    old_contents = old_pydeps_data[2:]
    new_contents = new_pydeps_data.splitlines()[2:]
    if old_pydeps_data[2:] != new_pydeps_data.splitlines()[2:]:
      return cmd, '\n'.join(difflib.context_diff(old_contents, new_contents))


def _CheckPydepsNeedsUpdating(input_api, output_api, checker_for_tests=None):
  """Checks if a .pydeps file needs to be regenerated."""
  # This check is for Python dependency lists (.pydeps files), and involves
  # paths not only in the PRESUBMIT.py, but also in the .pydeps files. It
  # doesn't work on Windows and Mac, so skip it on other platforms.
  if input_api.platform != 'linux2':
    return []
  # TODO(agrieve): Update when there's a better way to detect
  # this: crbug.com/570091
  is_android = input_api.os_path.exists('third_party/android_tools')
  pydeps_files = _ALL_PYDEPS_FILES if is_android else _GENERIC_PYDEPS_FILES
  results = []
  # First, check for new / deleted .pydeps.
  for f in input_api.AffectedFiles(include_deletes=True):
    # Check whether we are running the presubmit check for a file in src.
    # f.LocalPath is relative to repo (src, or internal repo).
    # os_path.exists is relative to src repo.
    # Therefore if os_path.exists is true, it means f.LocalPath is relative
    # to src and we can conclude that the pydeps is in src.
    if input_api.os_path.exists(f.LocalPath()):
      if f.LocalPath().endswith('.pydeps'):
        if f.Action() == 'D' and f.LocalPath() in _ALL_PYDEPS_FILES:
          results.append(output_api.PresubmitError(
              'Please update _ALL_PYDEPS_FILES within //PRESUBMIT.py to '
              'remove %s' % f.LocalPath()))
        elif f.Action() != 'D' and f.LocalPath() not in _ALL_PYDEPS_FILES:
          results.append(output_api.PresubmitError(
              'Please update _ALL_PYDEPS_FILES within //PRESUBMIT.py to '
              'include %s' % f.LocalPath()))

  if results:
    return results

  checker = checker_for_tests or PydepsChecker(input_api, pydeps_files)

  for pydep_path in checker.ComputeAffectedPydeps():
    try:
      result = checker.DetermineIfStale(pydep_path)
      if result:
        cmd, diff = result
        results.append(output_api.PresubmitError(
            'File is stale: %s\nDiff (apply to fix):\n%s\n'
            'To regenerate, run:\n\n    %s' %
            (pydep_path, diff, cmd)))
    except input_api.subprocess.CalledProcessError as error:
      return [output_api.PresubmitError('Error running: %s' % error.cmd,
          long_text=error.output)]

  return results


def _CheckSingletonInHeaders(input_api, output_api):
  """Checks to make sure no header files have |Singleton<|."""
  def FileFilter(affected_file):
    # It's ok for base/memory/singleton.h to have |Singleton<|.
    black_list = (_EXCLUDED_PATHS +
                  input_api.DEFAULT_BLACK_LIST +
                  (r"^base[\\/]memory[\\/]singleton\.h$",
                   r"^net[\\/]quic[\\/]platform[\\/]impl[\\/]"
                       r"quic_singleton_impl\.h$"))
    return input_api.FilterSourceFile(affected_file, black_list=black_list)

  pattern = input_api.re.compile(r'(?<!class\sbase::)Singleton\s*<')
  files = []
  for f in input_api.AffectedSourceFiles(FileFilter):
    if (f.LocalPath().endswith('.h') or f.LocalPath().endswith('.hxx') or
        f.LocalPath().endswith('.hpp') or f.LocalPath().endswith('.inl')):
      contents = input_api.ReadFile(f)
      for line in contents.splitlines(False):
        if (not line.lstrip().startswith('//') and # Strip C++ comment.
            pattern.search(line)):
          files.append(f)
          break

  if files:
    return [output_api.PresubmitError(
        'Found base::Singleton<T> in the following header files.\n' +
        'Please move them to an appropriate source file so that the ' +
        'template gets instantiated in a single compilation unit.',
        files) ]
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
def _CheckNoDeprecatedCss(input_api, output_api):
  """ Make sure that we don't use deprecated CSS
      properties, functions or values. Our external
      documentation and iOS CSS for dom distiller
      (reader mode) are ignored by the hooks as it
      needs to be consumed by WebKit. """
  results = []
  file_inclusion_pattern = [r".+\.css$"]
  black_list = (_EXCLUDED_PATHS +
                _TEST_CODE_EXCLUDED_PATHS +
                input_api.DEFAULT_BLACK_LIST +
                (r"^chrome/common/extensions/docs",
                 r"^chrome/docs",
                 r"^components/dom_distiller/core/css/distilledpage_ios.css",
                 r"^components/neterror/resources/neterror.css",
                 r"^native_client_sdk"))
  file_filter = lambda f: input_api.FilterSourceFile(
      f, white_list=file_inclusion_pattern, black_list=black_list)
  for fpath in input_api.AffectedFiles(file_filter=file_filter):
    for line_num, line in fpath.ChangedContents():
      for (deprecated_value, value) in _DEPRECATED_CSS:
        if deprecated_value in line:
          results.append(output_api.PresubmitError(
              "%s:%d: Use of deprecated CSS %s, use %s instead" %
              (fpath.LocalPath(), line_num, deprecated_value, value)))
  return results


_DEPRECATED_JS = [
  ( "__lookupGetter__", "Object.getOwnPropertyDescriptor" ),
  ( "__defineGetter__", "Object.defineProperty" ),
  ( "__defineSetter__", "Object.defineProperty" ),
]


# TODO: add unit tests
def _CheckNoDeprecatedJs(input_api, output_api):
  """Make sure that we don't use deprecated JS in Chrome code."""
  results = []
  file_inclusion_pattern = [r".+\.js$"]  # TODO(dbeam): .html?
  black_list = (_EXCLUDED_PATHS + _TEST_CODE_EXCLUDED_PATHS +
                input_api.DEFAULT_BLACK_LIST)
  file_filter = lambda f: input_api.FilterSourceFile(
      f, white_list=file_inclusion_pattern, black_list=black_list)
  for fpath in input_api.AffectedFiles(file_filter=file_filter):
    for lnum, line in fpath.ChangedContents():
      for (deprecated, replacement) in _DEPRECATED_JS:
        if deprecated in line:
          results.append(output_api.PresubmitError(
              "%s:%d: Use of deprecated JS %s, use %s instead" %
              (fpath.LocalPath(), lnum, deprecated, replacement)))
  return results


def _CheckForRiskyJsArrowFunction(line_number, line):
  if ' => ' in line:
    return "line %d, is using an => (arrow) function\n %s\n" % (
        line_number, line)
  return ''


def _CheckForRiskyJsConstLet(input_api, line_number, line):
  if input_api.re.match('^\s*(const|let)\s', line):
    return "line %d, is using const/let keyword\n %s\n" % (
        line_number, line)
  return ''


def _CheckForRiskyJsFeatures(input_api, output_api):
  maybe_ios_js = [r"^(ios|components|ui\/webui\/resources)\/.+\.js$"]
  # 'ui/webui/resources/cr_components are not allowed on ios'
  not_ios_filter = (r".*ui\/webui\/resources\/cr_components.*", )
  file_filter = lambda f: input_api.FilterSourceFile(f, white_list=maybe_ios_js,
                                                     black_list=not_ios_filter)
  results = []
  for f in input_api.AffectedFiles(file_filter=file_filter):
    arrow_error_lines = []
    const_let_error_lines = []
    for lnum, line in f.ChangedContents():
      arrow_error_lines += filter(None, [
        _CheckForRiskyJsArrowFunction(lnum, line),
      ])

      const_let_error_lines += filter(None, [
        _CheckForRiskyJsConstLet(input_api, lnum, line),
      ])

    if arrow_error_lines:
      arrow_error_lines = map(
          lambda e: "%s:%s" % (f.LocalPath(), e), arrow_error_lines)
      results.append(
          output_api.PresubmitPromptWarning('\n'.join(arrow_error_lines + [
"""
Use of => (arrow) operator detected in:
%s
Please ensure your code does not run on iOS9 (=> (arrow) does not work there).
https://chromium.googlesource.com/chromium/src/+/master/styleguide/web/es6.md#Arrow-Functions
""" % f.LocalPath()
          ])))

    if const_let_error_lines:
      const_let_error_lines = map(
          lambda e: "%s:%s" % (f.LocalPath(), e), const_let_error_lines)
      results.append(
          output_api.PresubmitPromptWarning('\n'.join(const_let_error_lines + [
"""
Use of const/let keywords detected in:
%s
Please ensure your code does not run on iOS9 because const/let is not fully
supported.
https://chromium.googlesource.com/chromium/src/+/master/styleguide/web/es6.md#let-Block_Scoped-Variables
https://chromium.googlesource.com/chromium/src/+/master/styleguide/web/es6.md#const-Block_Scoped-Constants
""" % f.LocalPath()
          ])))

  return results


def _CheckForRelativeIncludes(input_api, output_api):
  # Need to set the sys.path so PRESUBMIT_test.py runs properly
  import sys
  original_sys_path = sys.path
  try:
    sys.path = sys.path + [input_api.os_path.join(
        input_api.PresubmitLocalPath(), 'buildtools', 'checkdeps')]
    from cpp_checker import CppChecker
  finally:
    # Restore sys.path to what it was before.
    sys.path = original_sys_path

  bad_files = {}
  for f in input_api.AffectedFiles(include_deletes=False):
    if (f.LocalPath().startswith('third_party') and
      not f.LocalPath().startswith('third_party/WebKit') and
      not f.LocalPath().startswith('third_party\\WebKit')):
      continue

    if not CppChecker.IsCppFile(f.LocalPath()):
      continue

    relative_includes = [line for _, line in f.ChangedContents()
                         if "#include" in line and "../" in line]
    if not relative_includes:
      continue
    bad_files[f.LocalPath()] = relative_includes

  if not bad_files:
    return []

  error_descriptions = []
  for file_path, bad_lines in bad_files.iteritems():
    error_description = file_path
    for line in bad_lines:
      error_description += '\n    ' + line
    error_descriptions.append(error_description)

  results = []
  results.append(output_api.PresubmitError(
        'You added one or more relative #include paths (including "../").\n'
        'These shouldn\'t be used because they can be used to include headers\n'
        'from code that\'s not correctly specified as a dependency in the\n'
        'relevant BUILD.gn file(s).',
        error_descriptions))

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


def _CheckWatchlistsEntrySyntax(key, value, ast):
  if not isinstance(key, ast.Str):
    return 'Key at line %d must be a string literal' % key.lineno
  if not isinstance(value, ast.List):
    return 'Value at line %d must be a list' % value.lineno
  return None


def _CheckWATCHLISTSEntries(wd_dict, w_dict, ast):
  mismatch_template = (
      'Mismatch between WATCHLIST_DEFINITIONS entry (%s) and WATCHLISTS '
      'entry (%s)')

  i = 0
  last_key = ''
  while True:
    if i >= len(wd_dict.keys):
      if i >= len(w_dict.keys):
        return None
      return mismatch_template % ('missing', 'line %d' % w_dict.keys[i].lineno)
    elif i >= len(w_dict.keys):
      return (
          mismatch_template % ('line %d' % wd_dict.keys[i].lineno, 'missing'))

    wd_key = wd_dict.keys[i]
    w_key = w_dict.keys[i]

    result = _CheckWatchlistDefinitionsEntrySyntax(
        wd_key, wd_dict.values[i], ast)
    if result is not None:
      return 'Bad entry in WATCHLIST_DEFINITIONS dict: %s' % result

    result = _CheckWatchlistsEntrySyntax(w_key, w_dict.values[i], ast)
    if result is not None:
      return 'Bad entry in WATCHLISTS dict: %s' % result

    if wd_key.s != w_key.s:
      return mismatch_template % (
          '%s at line %d' % (wd_key.s, wd_key.lineno),
          '%s at line %d' % (w_key.s, w_key.lineno))

    if wd_key.s < last_key:
      return (
          'WATCHLISTS dict is not sorted lexicographically at line %d and %d' %
          (wd_key.lineno, w_key.lineno))
    last_key = wd_key.s

    i = i + 1


def _CheckWATCHLISTSSyntax(expression, ast):
  if not isinstance(expression, ast.Expression):
    return 'WATCHLISTS file must contain a valid expression'
  dictionary = expression.body
  if not isinstance(dictionary, ast.Dict) or len(dictionary.keys) != 2:
    return 'WATCHLISTS file must have single dict with exactly two entries'

  first_key = dictionary.keys[0]
  first_value = dictionary.values[0]
  second_key = dictionary.keys[1]
  second_value = dictionary.values[1]

  if (not isinstance(first_key, ast.Str) or
      first_key.s != 'WATCHLIST_DEFINITIONS' or
      not isinstance(first_value, ast.Dict)):
    return (
        'The first entry of the dict in WATCHLISTS file must be '
        'WATCHLIST_DEFINITIONS dict')

  if (not isinstance(second_key, ast.Str) or
      second_key.s != 'WATCHLISTS' or
      not isinstance(second_value, ast.Dict)):
    return (
        'The second entry of the dict in WATCHLISTS file must be '
        'WATCHLISTS dict')

  return _CheckWATCHLISTSEntries(first_value, second_value, ast)


def _CheckWATCHLISTS(input_api, output_api):
  for f in input_api.AffectedFiles(include_deletes=False):
    if f.LocalPath() == 'WATCHLISTS':
      contents = input_api.ReadFile(f, 'r')

      try:
        # First, make sure that it can be evaluated.
        input_api.ast.literal_eval(contents)
        # Get an AST tree for it and scan the tree for detailed style checking.
        expression = input_api.ast.parse(
            contents, filename='WATCHLISTS', mode='eval')
      except ValueError as e:
        return [output_api.PresubmitError(
            'Cannot parse WATCHLISTS file', long_text=repr(e))]
      except SyntaxError as e:
        return [output_api.PresubmitError(
            'Cannot parse WATCHLISTS file', long_text=repr(e))]
      except TypeError as e:
        return [output_api.PresubmitError(
            'Cannot parse WATCHLISTS file', long_text=repr(e))]

      result = _CheckWATCHLISTSSyntax(expression, input_api.ast)
      if result is not None:
        return [output_api.PresubmitError(result)]
      break

  return []


def _CheckNewHeaderWithoutGnChange(input_api, output_api):
  """Checks that newly added header files have corresponding GN changes.
  Note that this is only a heuristic. To be precise, run script:
  build/check_gn_headers.py.
  """

  def headers(f):
    return input_api.FilterSourceFile(
      f, white_list=(r'.+%s' % _HEADER_EXTENSIONS, ))

  new_headers = []
  for f in input_api.AffectedSourceFiles(headers):
    if f.Action() != 'A':
      continue
    new_headers.append(f.LocalPath())

  def gn_files(f):
    return input_api.FilterSourceFile(f, white_list=(r'.+\.gn', ))

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
    return [output_api.PresubmitPromptWarning(
      'Missing GN changes for new header files', items=sorted(problems),
      long_text='Please double check whether newly added header files need '
      'corresponding changes in gn or gni files.\nThis checking is only a '
      'heuristic. Run build/check_gn_headers.py to be precise.\n'
      'Read https://crbug.com/661774 for more info.')]
  return []


def _CheckCorrectProductNameInMessages(input_api, output_api):
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
    "filename_postfix": "chromium_strings.grd",
    "correct_name": "Chromium",
    "incorrect_name": "Chrome",
  }]

  for test_case in test_cases:
    problems = []
    filename_filter = lambda x: x.LocalPath().endswith(
        test_case["filename_postfix"])

    # Check each new line. Can yield false positives in multiline comments, but
    # easier than trying to parse the XML because messages can have nested
    # children, and associating message elements with affected lines is hard.
    for f in input_api.AffectedSourceFiles(filename_filter):
      for line_num, line in f.ChangedContents():
        if "<message" in line or "<!--" in line or "-->" in line:
          continue
        if test_case["incorrect_name"] in line:
          problems.append(
              "Incorrect product name in %s:%d" % (f.LocalPath(), line_num))

    if problems:
      message = (
        "Strings in %s-branded string files should reference \"%s\", not \"%s\""
            % (test_case["correct_name"], test_case["correct_name"],
               test_case["incorrect_name"]))
      all_problems.append(
          output_api.PresubmitPromptWarning(message, items=problems))

  return all_problems


def _AndroidSpecificOnUploadChecks(input_api, output_api):
  """Groups checks that target android code."""
  results = []
  results.extend(_CheckAndroidCrLogUsage(input_api, output_api))
  results.extend(_CheckAndroidNewMdpiAssetLocation(input_api, output_api))
  results.extend(_CheckAndroidToastUsage(input_api, output_api))
  results.extend(_CheckAndroidTestJUnitInheritance(input_api, output_api))
  results.extend(_CheckAndroidTestJUnitFrameworkImport(input_api, output_api))
  results.extend(_CheckAndroidTestAnnotationUsage(input_api, output_api))
  results.extend(_CheckAndroidWebkitImports(input_api, output_api))
  return results


def _CommonChecks(input_api, output_api):
  """Checks common to both upload and commit."""
  results = []
  results.extend(input_api.canned_checks.PanProjectChecks(
      input_api, output_api,
      excluded_paths=_EXCLUDED_PATHS))

  author = input_api.change.author_email
  if author and author not in _KNOWN_ROBOTS:
    results.extend(
        input_api.canned_checks.CheckAuthorizedAuthor(input_api, output_api))

  results.extend(
      _CheckNoProductionCodeUsingTestOnlyFunctions(input_api, output_api))
  results.extend(
      _CheckNoProductionCodeUsingTestOnlyFunctionsJava(input_api, output_api))
  results.extend(_CheckNoIOStreamInHeaders(input_api, output_api))
  results.extend(_CheckNoUNIT_TESTInSourceFiles(input_api, output_api))
  results.extend(_CheckNoDISABLETypoInTests(input_api, output_api))
  results.extend(_CheckDCHECK_IS_ONHasBraces(input_api, output_api))
  results.extend(_CheckNoNewWStrings(input_api, output_api))
  results.extend(_CheckNoDEPSGIT(input_api, output_api))
  results.extend(_CheckNoBannedFunctions(input_api, output_api))
  results.extend(_CheckNoPragmaOnce(input_api, output_api))
  results.extend(_CheckNoTrinaryTrueFalse(input_api, output_api))
  results.extend(_CheckUnwantedDependencies(input_api, output_api))
  results.extend(_CheckFilePermissions(input_api, output_api))
  results.extend(_CheckTeamTags(input_api, output_api))
  results.extend(_CheckNoAuraWindowPropertyHInHeaders(input_api, output_api))
  results.extend(_CheckForVersionControlConflicts(input_api, output_api))
  results.extend(_CheckPatchFiles(input_api, output_api))
  results.extend(_CheckHardcodedGoogleHostsInLowerLayers(input_api, output_api))
  results.extend(_CheckNoAbbreviationInPngFileName(input_api, output_api))
  results.extend(_CheckBuildConfigMacrosWithoutInclude(input_api, output_api))
  results.extend(_CheckForInvalidOSMacros(input_api, output_api))
  results.extend(_CheckForInvalidIfDefinedMacros(input_api, output_api))
  results.extend(_CheckFlakyTestUsage(input_api, output_api))
  results.extend(_CheckAddedDepsHaveTargetApprovals(input_api, output_api))
  results.extend(
      input_api.canned_checks.CheckChangeHasNoTabs(
          input_api,
          output_api,
          source_file_filter=lambda x: x.LocalPath().endswith('.grd')))
  results.extend(_CheckSpamLogging(input_api, output_api))
  results.extend(_CheckForAnonymousVariables(input_api, output_api))
  results.extend(_CheckUserActionUpdate(input_api, output_api))
  results.extend(_CheckNoDeprecatedCss(input_api, output_api))
  results.extend(_CheckNoDeprecatedJs(input_api, output_api))
  results.extend(_CheckParseErrors(input_api, output_api))
  results.extend(_CheckForIPCRules(input_api, output_api))
  results.extend(_CheckForLongPathnames(input_api, output_api))
  results.extend(_CheckForIncludeGuards(input_api, output_api))
  results.extend(_CheckForWindowsLineEndings(input_api, output_api))
  results.extend(_CheckSingletonInHeaders(input_api, output_api))
  results.extend(_CheckPydepsNeedsUpdating(input_api, output_api))
  results.extend(_CheckJavaStyle(input_api, output_api))
  results.extend(_CheckIpcOwners(input_api, output_api))
  results.extend(_CheckUselessForwardDeclarations(input_api, output_api))
  results.extend(_CheckForRiskyJsFeatures(input_api, output_api))
  results.extend(_CheckForRelativeIncludes(input_api, output_api))
  results.extend(_CheckWATCHLISTS(input_api, output_api))
  results.extend(input_api.RunTests(
    input_api.canned_checks.CheckVPythonSpec(input_api, output_api)))
  results.extend(_CheckTranslationScreenshots(input_api, output_api))
  results.extend(_CheckCorrectProductNameInMessages(input_api, output_api))

  for f in input_api.AffectedFiles():
    path, name = input_api.os_path.split(f.LocalPath())
    if name == 'PRESUBMIT.py':
      full_path = input_api.os_path.join(input_api.PresubmitLocalPath(), path)
      test_file = input_api.os_path.join(path, 'PRESUBMIT_test.py')
      if f.Action() != 'D' and input_api.os_path.exists(test_file):
        # The PRESUBMIT.py file (and the directory containing it) might
        # have been affected by being moved or removed, so only try to
        # run the tests if they still exist.
        results.extend(input_api.canned_checks.RunUnitTestsInDirectory(
            input_api, output_api, full_path,
            whitelist=[r'^PRESUBMIT_test\.py$']))
  return results


def _CheckPatchFiles(input_api, output_api):
  problems = [f.LocalPath() for f in input_api.AffectedFiles()
      if f.LocalPath().endswith(('.orig', '.rej'))]
  if problems:
    return [output_api.PresubmitError(
        "Don't commit .rej and .orig files.", problems)]
  else:
    return []


def _CheckBuildConfigMacrosWithoutInclude(input_api, output_api):
  # Excludes OS_CHROMEOS, which is not defined in build_config.h.
  macro_re = input_api.re.compile(r'^\s*#(el)?if.*\bdefined\(((OS_(?!CHROMEOS)|'
                                  'COMPILER_|ARCH_CPU_|WCHAR_T_IS_)[^)]*)')
  include_re = input_api.re.compile(
      r'^#include\s+"build/build_config.h"', input_api.re.MULTILINE)
  extension_re = input_api.re.compile(r'\.[a-z]+$')
  errors = []
  for f in input_api.AffectedFiles():
    if not f.LocalPath().endswith(('.h', '.c', '.cc', '.cpp', '.m', '.mm')):
      continue
    found_line_number = None
    found_macro = None
    for line_num, line in f.ChangedContents():
      match = macro_re.search(line)
      if match:
        found_line_number = line_num
        found_macro = match.group(2)
        break
    if not found_line_number:
      continue

    found_include = False
    for line in f.NewContents():
      if include_re.search(line):
        found_include = True
        break
    if found_include:
      continue

    if not f.LocalPath().endswith('.h'):
      primary_header_path = extension_re.sub('.h', f.AbsoluteLocalPath())
      try:
        content = input_api.ReadFile(primary_header_path, 'r')
        if include_re.search(content):
          continue
      except IOError:
        pass
    errors.append('%s:%d %s macro is used without including build/'
                  'build_config.h.'
                  % (f.LocalPath(), found_line_number, found_macro))
  if errors:
    return [output_api.PresubmitPromptWarning('\n'.join(errors))]
  return []


def _DidYouMeanOSMacro(bad_macro):
  try:
    return {'A': 'OS_ANDROID',
            'B': 'OS_BSD',
            'C': 'OS_CHROMEOS',
            'F': 'OS_FREEBSD',
            'L': 'OS_LINUX',
            'M': 'OS_MACOSX',
            'N': 'OS_NACL',
            'O': 'OS_OPENBSD',
            'P': 'OS_POSIX',
            'S': 'OS_SOLARIS',
            'W': 'OS_WIN'}[bad_macro[3].upper()]
  except KeyError:
    return ''


def _CheckForInvalidOSMacrosInFile(input_api, f):
  """Check for sensible looking, totally invalid OS macros."""
  preprocessor_statement = input_api.re.compile(r'^\s*#')
  os_macro = input_api.re.compile(r'defined\((OS_[^)]+)\)')
  results = []
  for lnum, line in f.ChangedContents():
    if preprocessor_statement.search(line):
      for match in os_macro.finditer(line):
        if not match.group(1) in _VALID_OS_MACROS:
          good = _DidYouMeanOSMacro(match.group(1))
          did_you_mean = ' (did you mean %s?)' % good if good else ''
          results.append('    %s:%d %s%s' % (f.LocalPath(),
                                             lnum,
                                             match.group(1),
                                             did_you_mean))
  return results


def _CheckForInvalidOSMacros(input_api, output_api):
  """Check all affected files for invalid OS macros."""
  bad_macros = []
  for f in input_api.AffectedFiles():
    if not f.LocalPath().endswith(('.py', '.js', '.html', '.css', '.md')):
      bad_macros.extend(_CheckForInvalidOSMacrosInFile(input_api, f))

  if not bad_macros:
    return []

  return [output_api.PresubmitError(
      'Possibly invalid OS macro[s] found. Please fix your code\n'
      'or add your macro to src/PRESUBMIT.py.', bad_macros)]


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
  ifdef_macro = input_api.re.compile(r'^\s*#.*(?:ifdef\s|defined\()([^\s\)]+)')
  results = []
  for lnum, line in f.ChangedContents():
    for match in ifdef_macro.finditer(line):
      if match.group(1) in ALWAYS_DEFINED_MACROS:
        always_defined = ' %s is always defined. ' % match.group(1)
        did_you_mean = 'Did you mean \'#if %s\'?' % match.group(1)
        results.append('    %s:%d %s\n\t%s' % (f.LocalPath(),
                                               lnum,
                                               always_defined,
                                               did_you_mean))
  return results


def _CheckForInvalidIfDefinedMacros(input_api, output_api):
  """Check all affected files for invalid "if defined" macros."""
  bad_macros = []
  for f in input_api.AffectedFiles():
    if f.LocalPath().startswith('third_party/sqlite/'):
      continue
    if f.LocalPath().endswith(('.h', '.c', '.cc', '.m', '.mm')):
      bad_macros.extend(_CheckForInvalidIfDefinedMacrosInFile(input_api, f))

  if not bad_macros:
    return []

  return [output_api.PresubmitError(
      'Found ifdef check on always-defined macro[s]. Please fix your code\n'
      'or check the list of ALWAYS_DEFINED_MACROS in src/PRESUBMIT.py.',
      bad_macros)]


def _CheckForIPCRules(input_api, output_api):
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
      if inclusion_pattern.search(line) and not comment_pattern.search(line):
        problems.append(
          '%s:%d\n    %s' % (local_path, line_number, line.strip()))

  if problems:
    return [output_api.PresubmitPromptWarning(
        _IPC_ENUM_TRAITS_DEPRECATED, problems)]
  else:
    return []


def _CheckForLongPathnames(input_api, output_api):
  """Check to make sure no files being submitted have long paths.
  This causes issues on Windows.
  """
  problems = []
  for f in input_api.AffectedSourceFiles(None):
    local_path = f.LocalPath()
    # Windows has a path limit of 260 characters. Limit path length to 200 so
    # that we have some extra for the prefix on dev machines and the bots.
    if len(local_path) > 200:
      problems.append(local_path)

  if problems:
    return [output_api.PresubmitError(_LONG_PATH_ERROR, problems)]
  else:
    return []


def _CheckForIncludeGuards(input_api, output_api):
  """Check that header files have proper guards against multiple inclusion.
  If a file should not have such guards (and it probably should) then it
  should include the string "no-include-guard-because-multiply-included".
  """
  def is_chromium_header_file(f):
    # We only check header files under the control of the Chromium
    # project. That is, those outside third_party apart from
    # third_party/blink.
    file_with_path = input_api.os_path.normpath(f.LocalPath())
    return (file_with_path.endswith('.h') and
            (not file_with_path.startswith('third_party') or
             file_with_path.startswith(
               input_api.os_path.join('third_party', 'blink'))))

  def replace_special_with_underscore(string):
    return input_api.re.sub(r'[+\\/.-]', '_', string)

  errors = []

  for f in input_api.AffectedSourceFiles(is_chromium_header_file):
    guard_name = None
    guard_line_number = None
    seen_guard_end = False

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
      r'\w*' + input_api.re.escape(replace_special_with_underscore(
        upper_base_file_name)) + r'\w*',
    ]
    guard_name_pattern = '|'.join(guard_name_pattern_list)
    guard_pattern = input_api.re.compile(
      r'#ifndef\s+(' + guard_name_pattern + ')')

    for line_number, line in enumerate(f.NewContents()):
      if 'no-include-guard-because-multiply-included' in line:
        guard_name = 'DUMMY'  # To not trigger check outside the loop.
        break

      if guard_name is None:
        match = guard_pattern.match(line)
        if match:
          guard_name = match.group(1)
          guard_line_number = line_number

          # We allow existing files to use include guards whose names
          # don't match the chromium style guide, but new files should
          # get it right.
          if not f.OldContents():
            if guard_name != expected_guard:
              errors.append(output_api.PresubmitPromptWarning(
                'Header using the wrong include guard name %s' % guard_name,
                ['%s:%d' % (f.LocalPath(), line_number + 1)],
                'Expected: %r\nFound: %r' % (expected_guard, guard_name)))
      else:
        # The line after #ifndef should have a #define of the same name.
        if line_number == guard_line_number + 1:
          expected_line = '#define %s' % guard_name
          if line != expected_line:
            errors.append(output_api.PresubmitPromptWarning(
              'Missing "%s" for include guard' % expected_line,
              ['%s:%d' % (f.LocalPath(), line_number + 1)],
              'Expected: %r\nGot: %r' % (expected_line, line)))

        if not seen_guard_end and line == '#endif  // %s' % guard_name:
          seen_guard_end = True
        elif seen_guard_end:
          if line.strip() != '':
            errors.append(output_api.PresubmitPromptWarning(
              'Include guard %s not covering the whole file' % (
                guard_name), [f.LocalPath()]))
            break  # Nothing else to check and enough to warn once.

    if guard_name is None:
      errors.append(output_api.PresubmitPromptWarning(
        'Missing include guard %s' % expected_guard,
        [f.LocalPath()],
        'Missing include guard in %s\n'
        'Recommended name: %s\n'
        'This check can be disabled by having the string\n'
        'no-include-guard-because-multiply-included in the header.' %
        (f.LocalPath(), expected_guard)))

  return errors


def _CheckForWindowsLineEndings(input_api, output_api):
  """Check source code and known ascii text files for Windows style line
  endings.
  """
  known_text_files = r'.*\.(txt|html|htm|mhtml|py|gyp|gypi|gn|isolate)$'

  file_inclusion_pattern = (
    known_text_files,
    r'.+%s' % _IMPLEMENTATION_EXTENSIONS
  )

  problems = []
  source_file_filter = lambda f: input_api.FilterSourceFile(
      f, white_list=file_inclusion_pattern, black_list=None)
  for f in input_api.AffectedSourceFiles(source_file_filter):
    include_file = False
    for _, line in f.ChangedContents():
      if line.endswith('\r\n'):
        include_file = True
    if include_file:
      problems.append(f.LocalPath())

  if problems:
    return [output_api.PresubmitPromptWarning('Are you sure that you want '
        'these files to contain Windows style line endings?\n' +
        '\n'.join(problems))]

  return []


def _CheckSyslogUseWarning(input_api, output_api, source_file_filter=None):
  """Checks that all source files use SYSLOG properly."""
  syslog_files = []
  for f in input_api.AffectedSourceFiles(source_file_filter):
    for line_number, line in f.ChangedContents():
      if 'SYSLOG' in line:
        syslog_files.append(f.LocalPath() + ':' + str(line_number))

  if syslog_files:
    return [output_api.PresubmitPromptWarning(
        'Please make sure there are no privacy sensitive bits of data in SYSLOG'
        ' calls.\nFiles to check:\n', items=syslog_files)]
  return []


def CheckChangeOnUpload(input_api, output_api):
  results = []
  results.extend(_CommonChecks(input_api, output_api))
  results.extend(_CheckValidHostsInDEPS(input_api, output_api))
  results.extend(
      input_api.canned_checks.CheckPatchFormatted(input_api, output_api))
  results.extend(_CheckUmaHistogramChanges(input_api, output_api))
  results.extend(_AndroidSpecificOnUploadChecks(input_api, output_api))
  results.extend(_CheckSyslogUseWarning(input_api, output_api))
  results.extend(_CheckGoogleSupportAnswerUrl(input_api, output_api))
  results.extend(_CheckUniquePtr(input_api, output_api))
  results.extend(_CheckNewHeaderWithoutGnChange(input_api, output_api))
  return results


def GetTryServerMasterForBot(bot):
  """Returns the Try Server master for the given bot.

  It tries to guess the master from the bot name, but may still fail
  and return None.  There is no longer a default master.
  """
  # Potentially ambiguous bot names are listed explicitly.
  master_map = {
      'chromium_presubmit': 'master.tryserver.chromium.linux',
      'tools_build_presubmit': 'master.tryserver.chromium.linux',
  }
  master = master_map.get(bot)
  if not master:
    if 'android' in bot:
      master = 'master.tryserver.chromium.android'
    elif 'linux' in bot or 'presubmit' in bot:
      master = 'master.tryserver.chromium.linux'
    elif 'win' in bot:
      master = 'master.tryserver.chromium.win'
    elif 'mac' in bot or 'ios' in bot:
      master = 'master.tryserver.chromium.mac'
  return master


def CheckChangeOnCommit(input_api, output_api):
  results = []
  results.extend(_CommonChecks(input_api, output_api))
  # Make sure the tree is 'open'.
  results.extend(input_api.canned_checks.CheckTreeIsOpen(
      input_api,
      output_api,
      json_url='http://chromium-status.appspot.com/current?format=json'))

  results.extend(
      input_api.canned_checks.CheckPatchFormatted(input_api, output_api))
  results.extend(input_api.canned_checks.CheckChangeHasBugField(
      input_api, output_api))
  results.extend(input_api.canned_checks.CheckChangeHasDescription(
      input_api, output_api))
  return results


def _CheckTranslationScreenshots(input_api, output_api):
  PART_FILE_TAG = "part"
  import os
  import sys
  from io import StringIO

  try:
    old_sys_path = sys.path
    sys.path = sys.path + [input_api.os_path.join(
          input_api.PresubmitLocalPath(), 'tools', 'grit')]
    import grit.grd_reader
    import grit.node.message
    import grit.util
  finally:
    sys.path = old_sys_path

  def _GetGrdMessages(grd_path_or_string, dir_path='.'):
    """Load the grd file and return a dict of message ids to messages.

    Ignores any nested grdp files pointed by <part> tag.
    """
    doc = grit.grd_reader.Parse(grd_path_or_string, dir_path,
        stop_after=None, first_ids_file=None,
        debug=False, defines=None,
        tags_to_ignore=set([PART_FILE_TAG]))
    return {
      msg.attrs['name']:msg for msg in doc.GetChildrenOfType(
        grit.node.message.MessageNode)
    }

  def _GetGrdpMessagesFromString(grdp_string):
    """Parses the contents of a grdp file given in grdp_string.

    grd_reader can't parse grdp files directly. Instead, this creates a
    temporary directory with a grd file pointing to the grdp file, and loads the
    grd from there. Any nested grdp files (pointed by <part> tag) are ignored.
    """
    WRAPPER = """<?xml version="1.0" encoding="utf-8"?>
    <grit latest_public_release="1" current_release="1">
      <release seq="1">
        <messages>
          <part file="sub.grdp" />
        </messages>
      </release>
    </grit>
    """
    with grit.util.TempDir({'main.grd': WRAPPER,
                            'sub.grdp': grdp_string}) as temp_dir:
      return _GetGrdMessages(temp_dir.GetPath('main.grd'), temp_dir.GetPath())

  new_or_added_paths = set(f.LocalPath()
      for f in input_api.AffectedFiles()
      if (f.Action() == 'A' or f.Action() == 'M'))
  removed_paths = set(f.LocalPath()
      for f in input_api.AffectedFiles(include_deletes=True)
      if f.Action() == 'D')

  affected_grds = [f for f in input_api.AffectedFiles()
      if (f.LocalPath().endswith('.grd') or
          f.LocalPath().endswith('.grdp'))]
  affected_png_paths = [f.AbsoluteLocalPath()
      for f in input_api.AffectedFiles()
      if (f.LocalPath().endswith('.png'))]

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
  missing_sha1 = []
  unnecessary_sha1_files = []


  def _CheckScreenshotAdded(screenshots_dir, message_id):
    sha1_path = input_api.os_path.join(
        screenshots_dir, message_id + '.png.sha1')
    if sha1_path not in new_or_added_paths:
      missing_sha1.append(sha1_path)


  def _CheckScreenshotRemoved(screenshots_dir, message_id):
    sha1_path = input_api.os_path.join(
        screenshots_dir, message_id + '.png.sha1')
    if sha1_path not in removed_paths:
      unnecessary_sha1_files.append(sha1_path)


  for f in affected_grds:
    file_path = f.LocalPath()
    old_id_to_msg_map = {}
    new_id_to_msg_map = {}
    if file_path.endswith('.grdp'):
      if f.OldContents():
        old_id_to_msg_map = _GetGrdpMessagesFromString(
          unicode('\n'.join(f.OldContents())))
      if f.NewContents():
        new_id_to_msg_map = _GetGrdpMessagesFromString(
          unicode('\n'.join(f.NewContents())))
    else:
      if f.OldContents():
        old_id_to_msg_map = _GetGrdMessages(
          StringIO(unicode('\n'.join(f.OldContents()))))
      if f.NewContents():
        new_id_to_msg_map = _GetGrdMessages(
          StringIO(unicode('\n'.join(f.NewContents()))))

    # Compute added, removed and modified message IDs.
    old_ids = set(old_id_to_msg_map)
    new_ids = set(new_id_to_msg_map)
    added_ids = new_ids - old_ids
    removed_ids = old_ids - new_ids
    modified_ids = set([])
    for key in old_ids.intersection(new_ids):
      if (old_id_to_msg_map[key].FormatXml()
          != new_id_to_msg_map[key].FormatXml()):
        modified_ids.add(key)

    grd_name, ext = input_api.os_path.splitext(
        input_api.os_path.basename(file_path))
    screenshots_dir = input_api.os_path.join(
        input_api.os_path.dirname(file_path), grd_name + ext.replace('.', '_'))

    # Check the screenshot directory for .png files. Warn if there is any.
    for png_path in affected_png_paths:
      if png_path.startswith(screenshots_dir):
        unnecessary_screenshots.append(png_path)

    for added_id in added_ids:
      _CheckScreenshotAdded(screenshots_dir, added_id)

    for modified_id in modified_ids:
      _CheckScreenshotAdded(screenshots_dir, modified_id)

    for removed_id in removed_ids:
      _CheckScreenshotRemoved(screenshots_dir, removed_id)

  results = []
  if unnecessary_screenshots:
    results.append(output_api.PresubmitNotifyResult(
      'Do not include actual screenshots in the changelist. Run '
      'tools/translate/upload_screenshots.py to upload them instead:',
      sorted(unnecessary_screenshots)))

  if missing_sha1:
    results.append(output_api.PresubmitNotifyResult(
      'You are adding or modifying UI strings.\n'
      'To ensure the best translations, take screenshots of the relevant UI '
      '(https://g.co/chrome/translation) and add these files to your '
      'changelist:', sorted(missing_sha1)))

  if unnecessary_sha1_files:
    results.append(output_api.PresubmitNotifyResult(
      'You removed strings associated with these files. Remove:',
      sorted(unnecessary_sha1_files)))

  return results
