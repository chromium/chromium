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
    r"^third_party[\\/]blink[\\/].*",
    r"^third_party[\\/]breakpad[\\/].*",
    # sqlite is an imported third party dependency.
    r"^third_party[\\/]sqlite[\\/].*",
    r"^v8[\\/].*",
    r".*MakeFile$",
    r".+_autogen\.h$",
    r".+_pb2\.py$",
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
    r'.+_(fuzz|fuzzer)(_[a-z]+)?%s' % _IMPLEMENTATION_EXTENSIONS,
    r'.+profile_sync_service_harness%s' % _IMPLEMENTATION_EXTENSIONS,
    r'.*[\\/](test|tool(s)?)[\\/].*',
    # content_shell is used for running layout tests.
    r'content[\\/]shell[\\/].*',
    # Non-production example code.
    r'mojo[\\/]examples[\\/].*',
    # Launcher for running iOS tests on the simulator.
    r'testing[\\/]iossim[\\/]iossim\.mm$',
    # EarlGrey app side code for tests.
    r'ios[\\/].*_app_interface\.mm$',
)

_THIRD_PARTY_EXCEPT_BLINK = 'third_party/(?!blink/)'

_TEST_ONLY_WARNING = (
    'You might be calling functions intended only for testing from\n'
    'production code.  It is OK to ignore this warning if you know what\n'
    'you are doing, as the heuristics used to detect the situation are\n'
    'not perfect.  The commit queue will not block on this warning,\n'
    'however the android-binary-size trybot will block if the method\n'
    'exists in the release apk.')


_INCLUDE_ORDER_WARNING = (
    'Your #include order seems to be broken. Remember to use the right '
    'collation (LC_COLLATE=C) and check\nhttps://google.github.io/styleguide/'
    'cppguide.html#Names_and_Order_of_Includes')


# Format: Sequence of tuples containing:
# * String pattern or, if starting with a slash, a regular expression.
# * Sequence of strings to show when the pattern matches.
# * Error flag. True if a match is a presubmit error, otherwise it's a warning.
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

# Format: Sequence of tuples containing:
# * String pattern or, if starting with a slash, a regular expression.
# * Sequence of strings to show when the pattern matches.
# * Error flag. True if a match is a presubmit error, otherwise it's a warning.
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
    (
      'freeWhenDone:NO',
      (
        'The use of "freeWhenDone:NO" with the NoCopy creation of ',
        'Foundation types is prohibited.',
      ),
      True,
    ),
)

# Format: Sequence of tuples containing:
# * String pattern or, if starting with a slash, a regular expression.
# * Sequence of strings to show when the pattern matches.
# * Error flag. True if a match is a presubmit error, otherwise it's a warning.
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

# Format: Sequence of tuples containing:
# * String pattern or, if starting with a slash, a regular expression.
# * Sequence of strings to show when the pattern matches.
# * Error flag. True if a match is a presubmit error, otherwise it's a warning.
_BANNED_IOS_EGTEST_FUNCTIONS = (
    (
      r'/\bEXPECT_OCMOCK_VERIFY\b',
      (
        'EXPECT_OCMOCK_VERIFY should not be used in EarlGrey tests because ',
        'it is meant for GTests. Use [mock verify] instead.'
      ),
      True,
    ),
)

# Directories that contain deprecated Bind() or Callback types.
# Find sub-directories from a given directory by running:
# for i in `find . -maxdepth 1 -type d`; do
#   echo "-- $i"
#   (cd $i; git grep -P 'base::(Bind\(|(Callback<|Closure))'|wc -l)
# done
#
# TODO(crbug.com/714018): Remove (or narrow the scope of) paths from this list
# when they have been converted to modern callback types (OnceCallback,
# RepeatingCallback, BindOnce, BindRepeating) in order to enable presubmit
# checks for them and prevent regressions.
_NOT_CONVERTED_TO_MODERN_BIND_AND_CALLBACK = '|'.join((
  '^android_webview/browser/',
  '^apps/',
  '^ash/',
  '^base/callback.h',  # Intentional.
  '^chrome/app/',
  '^chrome/browser/',
  '^chrome/chrome_elf/',
  '^chrome/chrome_watcher/',
  '^chrome/common/',
  '^chrome/installer/',
  '^chrome/notification_helper/',
  '^chrome/renderer/',
  '^chrome/services/',
  '^chrome/test/',
  '^chrome/tools/',
  '^chrome/utility/',
  '^chromecast/app/',
  '^chromecast/browser/',
  '^chromecast/crash/',
  '^chromecast/media/',
  '^chromecast/metrics/',
  '^chromecast/net/',
  '^chromeos/attestation/',
  '^chromeos/audio/',
  '^chromeos/components/',
  '^chromeos/cryptohome/',
  '^chromeos/dbus/',
  '^chromeos/geolocation/',
  '^chromeos/login/',
  '^chromeos/network/',
  '^chromeos/process_proxy/',
  '^chromeos/services/',
  '^chromeos/settings/',
  '^chromeos/timezone/',
  '^chromeos/tpm/',
  '^components/arc/',
  '^components/assist_ranker/',
  '^components/autofill/',
  '^components/autofill_assistant/',
  '^components/bookmarks/',
  '^components/browser_sync/',
  '^components/browser_watcher/',
  '^components/browsing_data/',
  '^components/cast_channel/',
  '^components/certificate_transparency/',
  '^components/chromeos_camera/',
  '^components/component_updater/',
  '^components/content_settings/',
  '^components/crash/',
  '^components/cronet/',
  '^components/data_reduction_proxy/',
  '^components/discardable_memory/',
  '^components/dom_distiller/',
  '^components/domain_reliability/',
  '^components/download/',
  '^components/drive/',
  '^components/exo/',
  '^components/favicon/',
  '^components/feature_engagement/',
  '^components/feedback/',
  '^components/flags_ui/',
  '^components/gcm_driver/',
  '^components/google/',
  '^components/guest_view/',
  '^components/heap_profiling/',
  '^components/history/',
  '^components/image_fetcher/',
  '^components/invalidation/',
  '^components/keyed_service/',
  '^components/login/',
  '^components/metrics/',
  '^components/metrics_services_manager/',
  '^components/nacl/',
  '^components/navigation_interception/',
  '^components/net_log/',
  '^components/network_time/',
  '^components/ntp_snippets/',
  '^components/ntp_tiles/',
  '^components/offline_items_collection/',
  '^components/offline_pages/',
  '^components/omnibox/',
  '^components/ownership/',
  '^components/pairing/',
  '^components/password_manager/',
  '^components/payments/',
  '^components/plugins/',
  '^components/policy/',
  '^components/pref_registry/',
  '^components/prefs/',
  '^components/proxy_config/',
  '^components/quirks/',
  '^components/rappor/',
  '^components/remote_cocoa/',
  '^components/renderer_context_menu/',
  '^components/rlz/',
  '^components/safe_browsing/',
  '^components/search_engines/',
  '^components/search_provider_logos/',
  '^components/security_interstitials/',
  '^components/security_state/',
  '^components/services/',
  '^components/sessions/',
  '^components/signin/',
  '^components/ssl_errors/',
  '^components/storage_monitor/',
  '^components/subresource_filter/',
  '^components/suggestions/',
  '^components/supervised_user_error_page/',
  '^components/sync/',
  '^components/sync_bookmarks/',
  '^components/sync_device_info/',
  '^components/sync_preferences/',
  '^components/sync_sessions/',
  '^components/test/',
  '^components/tracing/',
  '^components/translate/',
  '^components/ukm/',
  '^components/update_client/',
  '^components/upload_list/',
  '^components/variations/',
  '^components/visitedlink/',
  '^components/web_cache/',
  '^components/webcrypto/',
  '^components/webdata/',
  '^components/webdata_services/',
  '^components/wifi/',
  '^components/zoom/',
  '^content/app/',
  '^content/browser/',
  '^content/child/',
  '^content/common/',
  '^content/public/',
  '^content/renderer/android/',
  '^content/renderer/fetchers/',
  '^content/renderer/image_downloader/',
  '^content/renderer/input/',
  '^content/renderer/java/',
  '^content/renderer/media/',
  '^content/renderer/media_capture_from_element/',
  '^content/renderer/media_recorder/',
  '^content/renderer/p2p/',
  '^content/renderer/pepper/',
  '^content/renderer/service_worker/',
  '^content/renderer/worker/',
  '^content/test/',
  '^content/utility/',
  '^dbus/',
  '^device/base/',
  '^device/bluetooth/',
  '^device/fido/',
  '^device/gamepad/',
  '^device/vr/',
  '^extensions/',
  '^gin/',
  '^google_apis/dive/',
  '^google_apis/gaia/',
  '^google_apis/gcm/',
  '^headless/',
  '^ios/chrome/',
  '^ios/components/',
  '^ios/net/',
  '^ios/web/',
  '^ios/web_view/',
  '^ipc/',
  '^media/audio/',
  '^media/base/',
  '^media/capture/',
  '^media/cast/',
  '^media/cdm/',
  '^media/device_monitors/',
  '^media/ffmpeg/',
  '^media/filters/',
  '^media/formats/',
  '^media/gpu/',
  '^media/mojo/',
  '^media/muxers/',
  '^media/remoting/',
  '^media/renderers/',
  '^media/test/',
  '^mojo/core/',
  '^mojo/public/',
  '^net/',
  '^ppapi/proxy/',
  '^ppapi/shared_impl/',
  '^ppapi/tests/',
  '^ppapi/thunk/',
  '^remoting/base/',
  '^remoting/client/',
  '^remoting/codec/',
  '^remoting/host/',
  '^remoting/internal/',
  '^remoting/ios/',
  '^remoting/protocol/',
  '^remoting/signaling/',
  '^remoting/test/',
  '^sandbox/linux/',
  '^sandbox/win/',
  '^services/',
  '^storage/browser/',
  '^testing/libfuzzer/',
  '^third_party/blink/',
  '^third_party/crashpad/crashpad/test/gtest_main.cc',
  '^third_party/leveldatabase/leveldb_chrome.cc',
  '^third_party/boringssl/gtest_main_chromium.cc',
  '^third_party/cacheinvalidation/overrides/' +
     'google/cacheinvalidation/deps/callback.h',
  '^third_party/libaddressinput/chromium/chrome_address_validator.cc',
  '^third_party/zlib/google/',
  '^tools/android/',
  '^tools/clang/base_bind_rewriters/',  # Intentional.
  '^tools/gdb/gdb_chrome.py',  # Intentional.
  '^ui/accelerated_widget_mac/',
  '^ui/android/',
  '^ui/aura/',
  '^ui/base/',
  '^ui/compositor/',
  '^ui/display/',
  '^ui/events/',
  '^ui/gfx/',
  '^ui/message_center/',
  '^ui/snapshot/',
  '^ui/views_content_client/',
  '^ui/wm/',
))

# Format: Sequence of tuples containing:
# * String pattern or, if starting with a slash, a regular expression.
# * Sequence of strings to show when the pattern matches.
# * Error flag. True if a match is a presubmit error, otherwise it's a warning.
# * Sequence of paths to *not* check (regexps).
_BANNED_CPP_FUNCTIONS = (
    (
      r'/\bNULL\b',
      (
       'New code should not use NULL. Use nullptr instead.',
      ),
      False,
      (),
    ),
    (
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
      r'/XSelectInput|CWEventMask|XCB_CW_EVENT_MASK',
      (
       'Chrome clients wishing to select events on X windows should use',
       'ui::XScopedEventSelector.  It is safe to ignore this warning only if',
       'you are selecting events from the GPU process, or if you are using',
       'an XDisplay other than gfx::GetXDisplay().',
      ),
      True,
      (
        r"^ui[\\/]events[\\/]x[\\/].*\.cc$",
        r"^ui[\\/]gl[\\/].*\.cc$",
        r"^media[\\/]gpu[\\/].*\.cc$",
        r"^gpu[\\/].*\.cc$",
      ),
    ),
    (
      r'/XInternAtom|xcb_intern_atom',
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
          r'^base[\\/]third_party[\\/]symbolize[\\/].*',
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
        'TaskEnvironment::TimeSource::MOCK_TIME. There are still a',
        'few cases that may require a ScopedMockTimeMessageLoopTaskRunner',
        '(i.e. mocking the main MessageLoopForUI in browser_tests), but check',
        'with gab@ first if you think you need it)',
      ),
      False,
      (),
    ),
    (
      'std::regex',
      (
        'Using std::regex adds unnecessary binary size to Chrome. Please use',
        're2::RE2 instead (crbug.com/755321)',
      ),
      True,
      (),
    ),
    (
      r'/\bstd::stoi\b',
      (
        'std::stoi uses exceptions to communicate results. ',
        'Use base::StringToInt() instead.',
      ),
      True,
      [_THIRD_PARTY_EXCEPT_BLINK],  # Don't warn in third_party folders.
    ),
    (
      r'/\bstd::stol\b',
      (
        'std::stol uses exceptions to communicate results. ',
        'Use base::StringToInt() instead.',
      ),
      True,
      [_THIRD_PARTY_EXCEPT_BLINK],  # Don't warn in third_party folders.
    ),
    (
      r'/\bstd::stoul\b',
      (
        'std::stoul uses exceptions to communicate results. ',
        'Use base::StringToUint() instead.',
      ),
      True,
      [_THIRD_PARTY_EXCEPT_BLINK],  # Don't warn in third_party folders.
    ),
    (
      r'/\bstd::stoll\b',
      (
        'std::stoll uses exceptions to communicate results. ',
        'Use base::StringToInt64() instead.',
      ),
      True,
      [_THIRD_PARTY_EXCEPT_BLINK],  # Don't warn in third_party folders.
    ),
    (
      r'/\bstd::stoull\b',
      (
        'std::stoull uses exceptions to communicate results. ',
        'Use base::StringToUint64() instead.',
      ),
      True,
      [_THIRD_PARTY_EXCEPT_BLINK],  # Don't warn in third_party folders.
    ),
    (
      r'/\bstd::stof\b',
      (
        'std::stof uses exceptions to communicate results. ',
        'For locale-independent values, e.g. reading numbers from disk',
        'profiles, use base::StringToDouble().',
        'For user-visible values, parse using ICU.',
      ),
      True,
      [_THIRD_PARTY_EXCEPT_BLINK],  # Don't warn in third_party folders.
    ),
    (
      r'/\bstd::stod\b',
      (
        'std::stod uses exceptions to communicate results. ',
        'For locale-independent values, e.g. reading numbers from disk',
        'profiles, use base::StringToDouble().',
        'For user-visible values, parse using ICU.',
      ),
      True,
      [_THIRD_PARTY_EXCEPT_BLINK],  # Don't warn in third_party folders.
    ),
    (
      r'/\bstd::stold\b',
      (
        'std::stold uses exceptions to communicate results. ',
        'For locale-independent values, e.g. reading numbers from disk',
        'profiles, use base::StringToDouble().',
        'For user-visible values, parse using ICU.',
      ),
      True,
      [_THIRD_PARTY_EXCEPT_BLINK],  # Don't warn in third_party folders.
    ),
    (
      r'/\bstd::to_string\b',
      (
        'std::to_string is locale dependent and slower than alternatives.',
        'For locale-independent strings, e.g. writing numbers to disk',
        'profiles, use base::NumberToString().',
        'For user-visible strings, use base::FormatNumber() and',
        'the related functions in base/i18n/number_formatting.h.',
      ),
      False,  # Only a warning since it is already used.
      [_THIRD_PARTY_EXCEPT_BLINK],  # Don't warn in third_party folders.
    ),
    (
      r'/\bstd::shared_ptr\b',
      (
        'std::shared_ptr should not be used. Use scoped_refptr instead.',
      ),
      True,
      [_THIRD_PARTY_EXCEPT_BLINK,
       '^third_party/blink/renderer/core/typed_arrays/array_buffer/' +
         'array_buffer_contents\.(cc|h)'],
    ),
    (
      r'/\bstd::weak_ptr\b',
      (
        'std::weak_ptr should not be used. Use base::WeakPtr instead.',
      ),
      True,
      [_THIRD_PARTY_EXCEPT_BLINK],  # Not an error in third_party folders.
    ),
    (
      r'/\blong long\b',
      (
        'long long is banned. Use stdint.h if you need a 64 bit number.',
      ),
      False,  # Only a warning since it is already used.
      [_THIRD_PARTY_EXCEPT_BLINK],  # Don't warn in third_party folders.
    ),
    (
      r'/\bstd::bind\b',
      (
        'std::bind is banned because of lifetime risks.',
        'Use base::BindOnce or base::BindRepeating instead.',
      ),
      True,
      [_THIRD_PARTY_EXCEPT_BLINK],  # Not an error in third_party folders.
    ),
    (
      r'/\b#include <chrono>\b',
      (
        '<chrono> overlaps with Time APIs in base. Keep using',
        'base classes.',
      ),
      True,
      [_THIRD_PARTY_EXCEPT_BLINK],  # Not an error in third_party folders.
    ),
    (
      r'/\b#include <exception>\b',
      (
        'Exceptions are banned and disabled in Chromium.',
      ),
      True,
      [_THIRD_PARTY_EXCEPT_BLINK],  # Not an error in third_party folders.
    ),
    (
      r'/\bstd::function\b',
      (
        'std::function is banned. Instead use base::Callback which directly',
        'supports Chromium\'s weak pointers, ref counting and more.',
      ),
      False,  # Only a warning since it is already used.
      [_THIRD_PARTY_EXCEPT_BLINK],  # Do not warn in third_party folders.
    ),
    (
      r'/\b#include <random>\b',
      (
        'Do not use any random number engines from <random>. Instead',
        'use base::RandomBitGenerator.',
      ),
      True,
      [_THIRD_PARTY_EXCEPT_BLINK],  # Not an error in third_party folders.
    ),
    (
      r'/\bstd::ratio\b',
      (
        'std::ratio is banned by the Google Style Guide.',
      ),
      True,
      [_THIRD_PARTY_EXCEPT_BLINK],  # Not an error in third_party folders.
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
          'Please use base::Bind{Once,Repeating} instead',
          'of base::Bind. (crbug.com/714018)',
      ),
      False,
      _NOT_CONVERTED_TO_MODERN_BIND_AND_CALLBACK,
    ),
    (
      r'/\bbase::Callback[<:]',
      (
          'Please use base::{Once,Repeating}Callback instead',
          'of base::Callback. (crbug.com/714018)',
      ),
      False,
      _NOT_CONVERTED_TO_MODERN_BIND_AND_CALLBACK,
    ),
    (
      r'/\bbase::Closure\b',
      (
          'Please use base::{Once,Repeating}Closure instead',
          'of base::Closure. (crbug.com/714018)',
      ),
      False,
      _NOT_CONVERTED_TO_MODERN_BIND_AND_CALLBACK,
    ),
    (
      r'/base::SharedMemory(|Handle)',
      (
          'base::SharedMemory is deprecated. Please use',
          '{Writable,ReadOnly}SharedMemoryRegion instead.',
      ),
      False,
      (),
    ),
    (
      r'/\bRunMessageLoop\b',
      (
          'RunMessageLoop is deprecated, use RunLoop instead.',
      ),
      False,
      (),
    ),
    (
      'RunThisRunLoop',
      (
          'RunThisRunLoop is deprecated, use RunLoop directly instead.',
      ),
      False,
      (),
    ),
    (
      'RunAllPendingInMessageLoop()',
      (
          "Prefer RunLoop over RunAllPendingInMessageLoop, please contact gab@",
          "if you're convinced you need this.",
      ),
      False,
      (),
    ),
    (
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
    (
      r'MessageLoopRunner',
      (
          'MessageLoopRunner is deprecated, use RunLoop instead.',
      ),
      False,
      (),
    ),
    (
      'GetDeferredQuitTaskForRunLoop',
      (
          "GetDeferredQuitTaskForRunLoop shouldn't be needed, please contact",
          "gab@ if you found a use case where this is the only solution.",
      ),
      False,
      (),
    ),
    (
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
        r'^fuchsia/base/test_devtools_list_fetcher\.cc$',
      ),
    ),
    (
      'std::random_shuffle',
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
    (
      'GetAddressOf',
      (
        'Improper use of Microsoft::WRL::ComPtr<T>::GetAddressOf() has been ',
        'implicated in a few leaks. Use operator& instead. See ',
        'http://crbug.com/914910 for more conversion guidance.'
      ),
      True,
      (),
    ),
    (
      'DEFINE_TYPE_CASTS',
      (
        'DEFINE_TYPE_CASTS is deprecated. Instead, use downcast helpers from ',
        '//third_party/blink/renderer/platform/casting.h.'
      ),
      True,
      (
        r'^third_party/blink/renderer/.*\.(cc|h)$',
      ),
    ),
    (
      r'/\bIsHTML.+Element\(\b',
      (
        'Function IsHTMLXXXXElement is deprecated. Instead, use downcast ',
        ' helpers IsA<HTMLXXXXElement> from ',
        '//third_party/blink/renderer/platform/casting.h.'
      ),
      False,
      (
        r'^third_party/blink/renderer/.*\.(cc|h)$',
      ),
    ),
    (
      r'/\bToHTML.+Element(|OrNull)\(\b',
      (
        'Function ToHTMLXXXXElement and ToHTMLXXXXElementOrNull are '
        'deprecated. Instead, use downcast helpers To<HTMLXXXXElement> '
        'and DynamicTo<HTMLXXXXElement> from ',
        '//third_party/blink/renderer/platform/casting.h.'
        'auto* html_xxxx_ele = To<HTMLXXXXElement>(n)'
        'auto* html_xxxx_ele_or_null = DynamicTo<HTMLXXXXElement>(n)'
      ),
      False,
      (
        r'^third_party/blink/renderer/.*\.(cc|h)$',
      ),
    ),
    (
      r'/\bmojo::DataPipe\b',
      (
        'mojo::DataPipe is deprecated. Use mojo::CreateDataPipe instead.',
      ),
      True,
      (),
    ),
    (
      'SHFileOperation',
      (
        'SHFileOperation was deprecated in Windows Vista, and there are less ',
        'complex functions to achieve the same goals. Use IFileOperation for ',
        'any esoteric actions instead.'
      ),
      True,
      (),
    ),
    (
      'StringFromGUID2',
      (
        'StringFromGUID2 introduces an unnecessary dependency on ole32.dll.',
        'Use base::win::String16FromGUID instead.'
      ),
      True,
      (
        r'/base/win/win_util_unittest.cc'
      ),
    ),
    (
      'StringFromCLSID',
      (
        'StringFromCLSID introduces an unnecessary dependency on ole32.dll.',
        'Use base::win::String16FromGUID instead.'
      ),
      True,
      (
        r'/base/win/win_util_unittest.cc'
      ),
    ),
    (
      'kCFAllocatorNull',
      (
        'The use of kCFAllocatorNull with the NoCopy creation of ',
        'CoreFoundation types is prohibited.',
      ),
      True,
      (),
    ),
    (
      'mojo::ConvertTo',
      (
        'mojo::ConvertTo and TypeConverter are deprecated. Please consider',
        'StructTraits / UnionTraits / EnumTraits / ArrayTraits / MapTraits /',
        'StringTraits if you would like to convert between custom types and',
        'the wire format of mojom types.'
      ),
      False,
      (
        r'^fuchsia/engine/browser/url_request_rewrite_rules_manager\.cc$',
        r'^fuchsia/engine/url_request_rewrite_type_converters\.cc$',
        r'^third_party/blink/.*\.(cc|h)$',
        r'^content/renderer/.*\.(cc|h)$',
      ),
    ),
    (
      'CComPtr',
      (
        'New code should use Microsoft::WRL::ComPtr from wrl/client.h as a ',
        'replacement for CComPtr from ATL. See http://crbug.com/5027 for more ',
        'details.'
      ),
      False,
      (),
    ),
)

# Format: Sequence of tuples containing:
# * String pattern or, if starting with a slash, a regular expression.
# * Sequence of strings to show when the pattern matches.
_DEPRECATED_MOJO_TYPES = (
    (
      r'/\bmojo::AssociatedBinding\b',
      (
        'mojo::AssociatedBinding<Interface> is deprecated.',
        'Use mojo::AssociatedReceiver<Interface> instead.',
      ),
    ),
    (
      r'/\bmojo::AssociatedBindingSet\b',
      (
        'mojo::AssociatedBindingSet<Interface> is deprecated.',
        'Use mojo::AssociatedReceiverSet<Interface> instead.',
      ),
    ),
    (
      r'/\bmojo::AssociatedInterfacePtr\b',
      (
        'mojo::AssociatedInterfacePtr<Interface> is deprecated.',
        'Use mojo::AssociatedRemote<Interface> instead.',
      ),
    ),
    (
      r'/\bmojo::AssociatedInterfacePtrInfo\b',
      (
        'mojo::AssociatedInterfacePtrInfo<Interface> is deprecated.',
        'Use mojo::PendingAssociatedRemote<Interface> instead.',
      ),
    ),
    (
      r'/\bmojo::AssociatedInterfaceRequest\b',
      (
        'mojo::AssociatedInterfaceRequest<Interface> is deprecated.',
        'Use mojo::PendingAssociatedReceiver<Interface> instead.',
      ),
    ),
    (
      r'/\bmojo::Binding\b',
      (
        'mojo::Binding<Interface> is deprecated.',
        'Use mojo::Receiver<Interface> instead.',
      ),
    ),
    (
      r'/\bmojo::BindingSet\b',
      (
        'mojo::BindingSet<Interface> is deprecated.',
        'Use mojo::ReceiverSet<Interface> instead.',
      ),
    ),
    (
      r'/\bmojo::InterfacePtr\b',
      (
        'mojo::InterfacePtr<Interface> is deprecated.',
        'Use mojo::Remote<Interface> instead.',
      ),
    ),
    (
      r'/\bmojo::InterfacePtrInfo\b',
      (
        'mojo::InterfacePtrInfo<Interface> is deprecated.',
        'Use mojo::PendingRemote<Interface> instead.',
      ),
    ),
    (
      r'/\bmojo::InterfaceRequest\b',
      (
        'mojo::InterfaceRequest<Interface> is deprecated.',
        'Use mojo::PendingReceiver<Interface> instead.',
      ),
    ),
    (
      r'/\bmojo::MakeRequest\b',
      (
        'mojo::MakeRequest is deprecated.',
        'Use mojo::Remote::BindNewPipeAndPassReceiver() instead.',
      ),
    ),
    (
      r'/\bmojo::MakeRequestAssociatedWithDedicatedPipe\b',
      (
        'mojo::MakeRequest is deprecated.',
        'Use mojo::AssociatedRemote::'
        'BindNewEndpointAndPassDedicatedReceiverForTesting() instead.',
      ),
    ),
    (
      r'/\bmojo::MakeStrongBinding\b',
      (
        'mojo::MakeStrongBinding is deprecated.',
        'Either migrate to mojo::UniqueReceiverSet, if possible, or use',
        'mojo::MakeSelfOwnedReceiver() instead.',
      ),
    ),
    (
      r'/\bmojo::MakeStrongAssociatedBinding\b',
      (
        'mojo::MakeStrongAssociatedBinding is deprecated.',
        'Either migrate to mojo::UniqueAssociatedReceiverSet, if possible, or',
        'use mojo::MakeSelfOwnedAssociatedReceiver() instead.',
      ),
    ),
    (
      r'/\bmojo::StrongAssociatedBindingSet\b',
      (
        'mojo::StrongAssociatedBindingSet<Interface> is deprecated.',
        'Use mojo::UniqueAssociatedReceiverSet<Interface> instead.',
      ),
    ),
    (
      r'/\bmojo::StrongBindingSet\b',
      (
        'mojo::StrongBindingSet<Interface> is deprecated.',
        'Use mojo::UniqueReceiverSet<Interface> instead.',
      ),
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

# List of image extensions that are used as resources in chromium.
_IMAGE_EXTENSIONS = ['.svg', '.png', '.webp']

# These paths contain test data and other known invalid JSON files.
_KNOWN_INVALID_JSON_FILE_PATTERNS = [
    r'test[\\/]data[\\/]',
    r'^components[\\/]policy[\\/]resources[\\/]policy_templates\.json$',
    r'^third_party[\\/]protobuf[\\/]',
    r'^third_party[\\/]blink[\\/]renderer[\\/]devtools[\\/]protocol\.json$',
    r'^third_party[\\/]blink[\\/]web_tests[\\/]external[\\/]wpt[\\/]',
]


_VALID_OS_MACROS = (
    # Please keep sorted.
    'OS_AIX',
    'OS_ANDROID',
    'OS_ASMJS',
    'OS_BSD',
    'OS_CAT',       # For testing.
    'OS_CHROMEOS',
    'OS_CYGWIN',    # third_party code.
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
    'build/android/devil_chromium.pydeps',
    'build/android/gyp/aar.pydeps',
    'build/android/gyp/aidl.pydeps',
    'build/android/gyp/allot_native_libraries.pydeps',
    'build/android/gyp/apkbuilder.pydeps',
    'build/android/gyp/assert_static_initializers.pydeps',
    'build/android/gyp/bytecode_processor.pydeps',
    'build/android/gyp/compile_resources.pydeps',
    'build/android/gyp/create_app_bundle_apks.pydeps',
    'build/android/gyp/create_bundle_wrapper_script.pydeps',
    'build/android/gyp/copy_ex.pydeps',
    'build/android/gyp/create_app_bundle.pydeps',
    'build/android/gyp/create_apk_operations_script.pydeps',
    'build/android/gyp/create_java_binary_script.pydeps',
    'build/android/gyp/create_size_info_files.pydeps',
    'build/android/gyp/desugar.pydeps',
    'build/android/gyp/dexsplitter.pydeps',
    'build/android/gyp/dex.pydeps',
    'build/android/gyp/dist_aar.pydeps',
    'build/android/gyp/filter_zip.pydeps',
    'build/android/gyp/gcc_preprocess.pydeps',
    'build/android/gyp/generate_linker_version_script.pydeps',
    'build/android/gyp/ijar.pydeps',
    'build/android/gyp/jacoco_instr.pydeps',
    'build/android/gyp/java_cpp_enum.pydeps',
    'build/android/gyp/java_cpp_strings.pydeps',
    'build/android/gyp/javac.pydeps',
    'build/android/gyp/jinja_template.pydeps',
    'build/android/gyp/lint.pydeps',
    'build/android/gyp/main_dex_list.pydeps',
    'build/android/gyp/merge_manifest.pydeps',
    'build/android/gyp/prepare_resources.pydeps',
    'build/android/gyp/proguard.pydeps',
    'build/android/gyp/validate_static_library_dex_references.pydeps',
    'build/android/gyp/write_build_config.pydeps',
    'build/android/gyp/write_native_libraries_java.pydeps',
    'build/android/gyp/zip.pydeps',
    'build/android/incremental_install/generate_android_manifest.pydeps',
    'build/android/incremental_install/write_installer_json.pydeps',
    'build/android/resource_sizes.pydeps',
    'build/android/test_runner.pydeps',
    'build/android/test_wrapper/logdog_wrapper.pydeps',
    'build/protoc_java.pydeps',
    'chrome/android/features/create_stripped_java_factory.pydeps',
    'components/module_installer/android/module_desc_java.pydeps',
    'net/tools/testserver/testserver.pydeps',
    'testing/scripts/run_android_wpt.pydeps',
    'third_party/android_platform/development/scripts/stack.pydeps',
]


_GENERIC_PYDEPS_FILES = [
    'chrome/test/chromedriver/log_replay/client_replay_unittest.pydeps',
    'chrome/test/chromedriver/test/run_py_tests.pydeps',
    'third_party/blink/renderer/bindings/scripts/build_web_idl_database.pydeps',
    'third_party/blink/renderer/bindings/scripts/collect_idl_files.pydeps',
    'third_party/blink/renderer/bindings/scripts/generate_bindings.pydeps',
    'tools/binary_size/sizes.pydeps',
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
          for s in ('v8-ci-autoroll-builder', 'wpt-autoroller',)
  ) | set('%s@skia-public.iam.gserviceaccount.com' % s
          for s in ('chromium-autoroll',)
  ) | set('%s@skia-corp.google.com.iam.gserviceaccount.com' % s
          for s in ('chromium-internal-autoroll',))


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


def _FindHistogramNameInChunk(histogram_name, chunk):
  """Tries to find a histogram name or prefix in a line.

  Returns the existence of the histogram name, or None if it needs more chunk
  to determine."""
  # A histogram_suffixes tag type has an affected-histogram name as a prefix of
  # the histogram_name.
  if '<affected-histogram' in chunk:
    # If the tag is not completed, needs more chunk to get the name.
    if not '>' in chunk:
      return None
    if not 'name="' in chunk:
      return False
    # Retrieve the first portion of the chunk wrapped by double-quotations. We
    # expect the only attribute is the name.
    histogram_prefix = chunk.split('"')[1]
    return histogram_prefix in histogram_name
  # Typically the whole histogram name should in the line.
  return histogram_name in chunk


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
    chunk = ''
    for line_num, line in histograms_xml_modifications:
      chunk += line
      histogram_name_found = _FindHistogramNameInChunk(histogram_info[0], chunk)
      if histogram_name_found is None:
        continue
      chunk = ''
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
        chunk = ''
        for line in histograms_xml:
          chunk += line
          histogram_name_found = _FindHistogramNameInChunk(histogram_name,
                                                           chunk)
          if histogram_name_found is None:
            continue
          chunk = ''
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
    gclient_path = input_api.os_path.join(
        input_api.PresubmitLocalPath(),
        'third_party', 'depot_tools', 'gclient.py')
    input_api.subprocess.check_output(
        [input_api.python_executable, gclient_path, 'verify'],
        stderr=input_api.subprocess.STDOUT)
    return []
  except input_api.subprocess.CalledProcessError as error:
    return [output_api.PresubmitError(
        'DEPS file must have only git dependencies.',
        long_text=error.output)]


def _GetMessageForMatchingType(input_api, affected_file, line_number, line,
                               type_name, message):
  """Helper method for _CheckNoBannedFunctions and _CheckNoDeprecatedMojoTypes.

  Returns an string composed of the name of the file, the line number where the
  match has been found and the additional text passed as |message| in case the
  target type name matches the text inside the line passed as parameter.
  """
  matched = False
  if type_name[0:1] == '/':
    regex = type_name[1:]
    if input_api.re.search(regex, line):
      matched = True
  elif type_name in line:
    matched = True

  result = []
  if matched:
    result.append('    %s:%d:' % (affected_file.LocalPath(), line_number))
    for message_line in message:
      result.append('      %s' % message_line)

  return result


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

  def IsIosObjcFile(affected_file):
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
    problems = _GetMessageForMatchingType(input_api, f, line_num, line,
                                          func_name, message)
    if problems:
      if error:
        errors.extend(problems)
      else:
        warnings.extend(problems)

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

  for f in input_api.AffectedFiles(file_filter=IsIosObjcFile):
    for line_num, line in f.ChangedContents():
      for func_name, message, error in _BANNED_IOS_OBJC_FUNCTIONS:
        CheckForMatch(f, line_num, line, func_name, message, error)

  egtest_filter = lambda f: f.LocalPath().endswith(('_egtest.mm'))
  for f in input_api.AffectedFiles(file_filter=egtest_filter):
    for line_num, line in f.ChangedContents():
      for func_name, message, error in _BANNED_IOS_EGTEST_FUNCTIONS:
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


def _CheckNoDeprecatedMojoTypes(input_api, output_api):
  """Make sure that old Mojo types are not used."""
  warnings = []

  file_filter = lambda f: f.LocalPath().endswith(('.cc', '.mm', '.h'))
  for f in input_api.AffectedFiles(file_filter=file_filter):
    # Only need to check Blink for warnings for now.
    if not f.LocalPath().startswith('third_party/blink'):
      continue

    for line_num, line in f.ChangedContents():
      for func_name, message in _DEPRECATED_MOJO_TYPES:
        problems = _GetMessageForMatchingType(input_api, f, line_num, line,
                                              func_name, message)
        if problems:
            warnings.extend(problems)

  result = []
  if (warnings):
    result.append(output_api.PresubmitPromptWarning(
        'Banned Mojo types were used.\n' + '\n'.join(warnings)))
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
      warnings = input_api.subprocess.check_output(args + files).splitlines()
      if warnings:
        return [output_api.PresubmitPromptWarning(warnings[0], warnings[1:])]
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
    if f.LocalPath().endswith(('.md', '.rst', '.txt')):
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


def _CheckChromeOsSyncedPrefRegistration(input_api, output_api):
  """Warns if Chrome OS C++ files register syncable prefs as browser prefs."""
  def FileFilter(affected_file):
    """Includes directories known to be Chrome OS only."""
    return input_api.FilterSourceFile(
      affected_file,
      white_list=('^ash/',
                  '^chromeos/',  # Top-level src/chromeos.
                  '/chromeos/',  # Any path component.
                  '^components/arc',
                  '^components/exo'),
      black_list=(input_api.DEFAULT_BLACK_LIST))

  prefs = []
  priority_prefs = []
  for f in input_api.AffectedFiles(file_filter=FileFilter):
    for line_num, line in f.ChangedContents():
      if input_api.re.search('PrefRegistrySyncable::SYNCABLE_PREF', line):
        prefs.append('    %s:%d:' % (f.LocalPath(), line_num))
        prefs.append('      %s' % line)
      if input_api.re.search(
          'PrefRegistrySyncable::SYNCABLE_PRIORITY_PREF', line):
        priority_prefs.append('    %s:%d' % (f.LocalPath(), line_num))
        priority_prefs.append('      %s' % line)

  results = []
  if (prefs):
    results.append(output_api.PresubmitPromptWarning(
        'Preferences were registered as SYNCABLE_PREF and will be controlled '
        'by browser sync settings. If these prefs should be controlled by OS '
        'sync settings use SYNCABLE_OS_PREF instead.\n' + '\n'.join(prefs)))
  if (priority_prefs):
    results.append(output_api.PresubmitPromptWarning(
        'Preferences were registered as SYNCABLE_PRIORITY_PREF and will be '
        'controlled by browser sync settings. If these prefs should be '
        'controlled by OS sync settings use SYNCABLE_OS_PRIORITY_PREF '
        'instead.\n' + '\n'.join(prefs)))
  return results


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
      r"^third_party[\\/]blink[\\/].*", f.LocalPath())
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
                 r"^base[\\/]task[\\/]thread_pool[\\/]task_tracker\.cc$",
                 r"^chrome[\\/]app[\\/]chrome_main_delegate\.cc$",
                 r"^chrome[\\/]browser[\\/]chrome_browser_main\.cc$",
                 r"^chrome[\\/]browser[\\/]ui[\\/]startup[\\/]"
                     r"startup_browser_creator\.cc$",
                 r"^chrome[\\/]browser[\\/]browser_switcher[\\/]bho[\\/].*",
                 r"^chrome[\\/]browser[\\/]diagnostics[\\/]" +
                     r"diagnostics_writer\.cc$",
                 r"^chrome[\\/]chrome_cleaner[\\/].*",
                 r"^chrome[\\/]chrome_elf[\\/]dll_hash[\\/]dll_hash_main\.cc$",
                 r"^chrome[\\/]installer[\\/]setup[\\/].*",
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
                 r"^fuchsia[\\/]engine[\\/]browser[\\/]frame_impl.cc$",
                 r"^fuchsia[\\/]engine[\\/]context_provider_main.cc$",
                 r"^headless[\\/]app[\\/]headless_shell\.cc$",
                 r"^ipc[\\/]ipc_logging\.cc$",
                 r"^native_client_sdk[\\/]",
                 r"^remoting[\\/]base[\\/]logging\.h$",
                 r"^remoting[\\/]host[\\/].*",
                 r"^sandbox[\\/]linux[\\/].*",
                 r"^storage[\\/]browser[\\/]file_system[\\/]" +
                     r"dump_file_system.cc$",
                 r"^tools[\\/]",
                 r"^ui[\\/]base[\\/]resource[\\/]data_pack.cc$",
                 r"^ui[\\/]aura[\\/]bench[\\/]bench_main\.cc$",
                 r"^ui[\\/]ozone[\\/]platform[\\/]cast[\\/]"))
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


def _CheckPythonDevilInit(input_api, output_api):
  """Checks to make sure devil is initialized correctly in python scripts."""
  script_common_initialize_pattern = input_api.re.compile(
      r'script_common\.InitializeEnvironment\(')
  devil_env_config_initialize = input_api.re.compile(
      r'devil_env\.config\.Initialize\(')

  errors = []

  sources = lambda affected_file: input_api.FilterSourceFile(
      affected_file,
      black_list=(_EXCLUDED_PATHS + input_api.DEFAULT_BLACK_LIST +
                  (r'^build[\\/]android[\\/]devil_chromium\.py',
                   r'^third_party[\\/].*',)),
      white_list=[r'.*\.py$'])

  for f in input_api.AffectedSourceFiles(sources):
    for line_num, line in f.ChangedContents():
      if (script_common_initialize_pattern.search(line) or
          devil_env_config_initialize.search(line)):
        errors.append("%s:%d" % (f.LocalPath(), line_num))

  results = []

  if errors:
    results.append(output_api.PresubmitError(
        'Devil initialization should always be done using '
        'devil_chromium.Initialize() in the chromium project, to use better '
        'defaults for dependencies (ex. up-to-date version of adb).',
        errors))

  return results


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
      # These files are just used to communicate between class loaders running
      # in the same process.
      'weblayer/browser/java/org/chromium/weblayer_private/interfaces/*',
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
    # affected files for .json, .cc, and .h files which look like they contain
    # a manifest definition.
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
    else:
      manifest_pattern = input_api.re.compile('manifests?\.(cc|h)$')
      test_manifest_pattern = input_api.re.compile('test_manifests?\.(cc|h)')
      if (manifest_pattern.search(f.LocalPath()) and not
          test_manifest_pattern.search(f.LocalPath())):
        # We expect all actual service manifest files to contain at least one
        # qualified reference to service_manager::Manifest.
        if 'service_manager::Manifest' in '\n'.join(f.NewContents()):
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


def _CheckSetNoParent(input_api, output_api):
  """Checks that set noparent is only used together with an OWNERS file in
     //build/OWNERS.setnoparent (see also
     //docs/code_reviews.md#owners-files-details)
  """
  errors = []

  allowed_owners_files_file = 'build/OWNERS.setnoparent'
  allowed_owners_files = set()
  with open(allowed_owners_files_file, 'r') as f:
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
    # listed in build/OWNERS.setnoparent.
    for set_noparent_line in found_set_noparent_lines:
      if set_noparent_line in found_owners_files:
        continue
      errors.append('  %s:%d' % (f.LocalPath(),
                                 found_set_noparent_lines[set_noparent_line]))

  results = []
  if errors:
    if input_api.is_committing:
      output = output_api.PresubmitError
    else:
      output = output_api.PresubmitPromptWarning
    results.append(output(
        'Found the following "set noparent" restrictions in OWNERS files that '
        'do not include owners from build/OWNERS.setnoparent:',
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
        not f.LocalPath().startswith('third_party\\blink')):
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
      black_list=(_EXCLUDED_PATHS +
                  _TEST_CODE_EXCLUDED_PATHS +
                  input_api.DEFAULT_BLACK_LIST +
                  (r"^android_webview[\\/]support_library[\\/]"
                      "boundary_interfaces[\\/]",
                   r"^chrome[\\/]android[\\/]webapk[\\/].*",
                   r'^third_party[\\/].*',
                   r"tools[\\/]android[\\/]customtabs_benchmark[\\/].*",
                   r"webview[\\/]chromium[\\/]License.*",)),
      white_list=[r'.*\.java$'])

  for f in input_api.AffectedSourceFiles(sources):
    for line_num, line in f.ChangedContents():
      if build_type_check_pattern.search(line):
        errors.append("%s:%d" % (f.LocalPath(), line_num))

  results = []

  if errors:
    results.append(output_api.PresubmitPromptWarning(
        'Build.TYPE.equals or .equals(Build.TYPE) usage is detected.'
        ' Please use BuildInfo.isDebugAndroid() instead.',
        errors))

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
      r'static final String TAG = "(?P<name>(.*))"')

  REF_MSG = ('See docs/android_logging.md for more info.')
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
     android.webview.ValueCallback except in the WebView glue layer
     and WebLayer.
  """
  valuecallback_import_pattern = input_api.re.compile(
      r'^import android\.webkit\.ValueCallback;$')

  errors = []

  sources = lambda affected_file: input_api.FilterSourceFile(
      affected_file,
      black_list=(_EXCLUDED_PATHS +
                  _TEST_CODE_EXCLUDED_PATHS +
                  input_api.DEFAULT_BLACK_LIST +
                  (r'^android_webview[\\/]glue[\\/].*',
                   r'^weblayer[\\/].*',)),
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


def _CheckAndroidXmlStyle(input_api, output_api, is_check_on_upload):
  """Checks Android XML styles """
  import sys
  original_sys_path = sys.path
  try:
    sys.path = sys.path + [input_api.os_path.join(
        input_api.PresubmitLocalPath(), 'tools', 'android', 'checkxmlstyle')]
    import checkxmlstyle
  finally:
    # Restore sys.path to what it was before.
    sys.path = original_sys_path

  if is_check_on_upload:
    return checkxmlstyle.CheckStyleOnUpload(input_api, output_api)
  else:
    return checkxmlstyle.CheckStyleOnCommit(input_api, output_api)


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
      # Changes to DEPS can lead to .pydeps changes if any .py files are in
      # subrepositories. We can't figure out which files change, so re-check
      # all files.
      # Changes to print_python_deps.py affect all .pydeps.
      if local_path == 'DEPS' or local_path.endswith('print_python_deps.py'):
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


def _CheckPydepsNeedsUpdating(input_api, output_api, checker_for_tests=None):
  """Checks if a .pydeps file needs to be regenerated."""
  # This check is for Python dependency lists (.pydeps files), and involves
  # paths not only in the PRESUBMIT.py, but also in the .pydeps files. It
  # doesn't work on Windows and Mac, so skip it on other platforms.
  if input_api.platform != 'linux2':
    return []
  is_android = _ParseGclientArgs().get('checkout_android', 'false') == 'true'
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


def _CheckForRelativeIncludes(input_api, output_api):
  bad_files = {}
  for f in input_api.AffectedFiles(include_deletes=False):
    if (f.LocalPath().startswith('third_party') and
      not f.LocalPath().startswith('third_party/blink') and
      not f.LocalPath().startswith('third_party\\blink')):
      continue

    if not _IsCPlusPlusFile(input_api, f.LocalPath()):
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


def _CheckForCcIncludes(input_api, output_api):
  """Check that nobody tries to include a cc file. It's a relatively
  common error which results in duplicate symbols in object
  files. This may not always break the build until someone later gets
  very confusing linking errors."""
  results = []
  for f in input_api.AffectedFiles(include_deletes=False):
    # We let third_party code do whatever it wants
    if (f.LocalPath().startswith('third_party') and
      not f.LocalPath().startswith('third_party/blink') and
      not f.LocalPath().startswith('third_party\\blink')):
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
            results.append(output_api.PresubmitError(
              'Only header files or .inc files should be included in other\n'
              'C++ files. Compiling the contents of a cc file more than once\n'
              'will cause duplicate information in the build which may later\n'
              'result in strange link_errors.\n' +
              f.LocalPath() + ':\n    ' +
              line))

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
      return ('Watchlist element on line %d doesn\'t look like a valid ' +
              'email: %s') % (key.lineno, element.s)
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

    result = _CheckWatchlistsEntrySyntax(
        w_key, w_dict.values[i], ast, email_regex)
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

  return _CheckWATCHLISTSEntries(first_value, second_value, input_api)


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

      result = _CheckWATCHLISTSSyntax(expression, input_api)
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


def _CheckBuildtoolsRevisionsAreInSync(input_api, output_api):
  # TODO(crbug.com/941824): We need to make sure the entries in
  # //buildtools/DEPS are kept in sync with the entries in //DEPS
  # so that users of //buildtools in other projects get the same tooling
  # Chromium gets. If we ever fix the referenced bug and add 'includedeps'
  # support to gclient, we can eliminate the duplication and delete
  # this presubmit check.

  # Update this regexp if new revisions are added to the files.
  rev_regexp = input_api.re.compile(
      "'((clang_format|libcxx|libcxxabi|libunwind)_revision|gn_version)':")

  # If a user is changing one revision, they need to change the same
  # line in both files. This means that any given change should contain
  # exactly the same list of changed lines that match the regexps. The
  # replace(' ', '') call allows us to ignore whitespace changes to the
  # lines. The 'long_text' parameter to the error will contain the
  # list of changed lines in both files, which should make it easy enough
  # to spot the error without going overboard in this implementation.
  revs_changes = {
      'DEPS': {},
      'buildtools/DEPS': {},
  }
  long_text = ''

  for f in input_api.AffectedFiles(
      file_filter=lambda f: f.LocalPath() in ('DEPS', 'buildtools/DEPS')):
    for line_num, line in f.ChangedContents():
      if rev_regexp.search(line):
        revs_changes[f.LocalPath()][line.replace(' ', '')] = line
        long_text += '%s:%d: %s\n' % (f.LocalPath(), line_num, line)

  if set(revs_changes['DEPS']) != set(revs_changes['buildtools/DEPS']):
    return [output_api.PresubmitError(
        'Change buildtools revisions in sync in both //DEPS and '
        '//buildtools/DEPS.', long_text=long_text + '\n')]
  else:
    return []


def _CheckForTooLargeFiles(input_api, output_api):
  """Avoid large files, especially binary files, in the repository since
  git doesn't scale well for those. They will be in everyone's repo
  clones forever, forever making Chromium slower to clone and work
  with."""

  # Uploading files to cloud storage is not trivial so we don't want
  # to set the limit too low, but the upper limit for "normal" large
  # files seems to be 1-2 MB, with a handful around 5-8 MB, so
  # anything over 20 MB is exceptional.
  TOO_LARGE_FILE_SIZE_LIMIT = 20 * 1024 * 1024  # 10 MB

  too_large_files = []
  for f in input_api.AffectedFiles():
    # Check both added and modified files (but not deleted files).
    if f.Action() in ('A', 'M'):
      size = input_api.os_path.getsize(f.AbsoluteLocalPath())
      if size > TOO_LARGE_FILE_SIZE_LIMIT:
        too_large_files.append("%s: %d bytes" % (f.LocalPath(), size))

  if too_large_files:
    message = (
      'Do not commit large files to git since git scales badly for those.\n' +
      'Instead put the large files in cloud storage and use DEPS to\n' +
      'fetch them.\n' + '\n'.join(too_large_files)
    )
    return [output_api.PresubmitError(
        'Too large files found in commit', long_text=message + '\n')]
  else:
    return []


def _CheckFuzzTargets(input_api, output_api):
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
    white_list = r'.*fuzz.*\.(h|hpp|hcc|cc|cpp|cxx)$'
    black_list = r"^third_party[\\/]libFuzzer"

    return input_api.FilterSourceFile(
        affected_file,
        white_list=[white_list],
        black_list=[black_list])

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
      'https://chromium.googlesource.com/chromium/src/+/master/testing/'
      'libfuzzer/efficient_fuzzing.md#simplifying-initialization_cleanup.\n' % (
          ', '.join(EXPORTED_SYMBOLS), REQUIRED_HEADER)
    )

  return [output_api.PresubmitPromptWarning(
        message="Missing '%s' in:" % REQUIRED_HEADER,
        items=files_with_missing_header,
        long_text=long_text)]


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
    black_list=(('(?i).*test', r'.*\/junit\/')
                + input_api.DEFAULT_BLACK_LIST),
    white_list=[r'.*\/(drawable|mipmap)' ]
  )
  for f in input_api.AffectedFiles(
      include_deletes=False, file_filter=filter_lambda):
    local_path = f.LocalPath().lower()
    if any(local_path.endswith(extension) for extension in _IMAGE_EXTENSIONS):
      images_added = True
      image_paths.append(f)
  if images_added:
    errors.append(output_api.PresubmitPromptWarning(
        'It looks like you are trying to commit some images. If these are '
        'non-test-only images, please make sure to read and apply the tips in '
        'https://chromium.googlesource.com/chromium/src/+/HEAD/docs/speed/'
        'binary_size/optimization_advice.md#optimizing-images\nThis check is '
        'FYI only and will not block your CL on the CQ.', image_paths))
  return errors


def _AndroidSpecificOnUploadChecks(input_api, output_api):
  """Groups upload checks that target android code."""
  results = []
  results.extend(_CheckAndroidCrLogUsage(input_api, output_api))
  results.extend(_CheckAndroidDebuggableBuild(input_api, output_api))
  results.extend(_CheckAndroidNewMdpiAssetLocation(input_api, output_api))
  results.extend(_CheckAndroidToastUsage(input_api, output_api))
  results.extend(_CheckAndroidTestJUnitInheritance(input_api, output_api))
  results.extend(_CheckAndroidTestJUnitFrameworkImport(input_api, output_api))
  results.extend(_CheckAndroidTestAnnotationUsage(input_api, output_api))
  results.extend(_CheckAndroidWebkitImports(input_api, output_api))
  results.extend(_CheckAndroidXmlStyle(input_api, output_api, True))
  results.extend(_CheckNewImagesWarning(input_api, output_api))
  return results

def _AndroidSpecificOnCommitChecks(input_api, output_api):
  """Groups commit checks that target android code."""
  results = []
  results.extend(_CheckAndroidXmlStyle(input_api, output_api, False))
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
  results.extend(_CheckNoDeprecatedMojoTypes(input_api, output_api))
  results.extend(_CheckNoPragmaOnce(input_api, output_api))
  results.extend(_CheckNoTrinaryTrueFalse(input_api, output_api))
  results.extend(_CheckUnwantedDependencies(input_api, output_api))
  results.extend(_CheckFilePermissions(input_api, output_api))
  results.extend(_CheckTeamTags(input_api, output_api))
  results.extend(_CheckNoAuraWindowPropertyHInHeaders(input_api, output_api))
  results.extend(_CheckForVersionControlConflicts(input_api, output_api))
  results.extend(_CheckPatchFiles(input_api, output_api))
  results.extend(_CheckHardcodedGoogleHostsInLowerLayers(input_api, output_api))
  results.extend(_CheckChromeOsSyncedPrefRegistration(input_api, output_api))
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
  results.extend(_CheckParseErrors(input_api, output_api))
  results.extend(_CheckForIPCRules(input_api, output_api))
  results.extend(_CheckForLongPathnames(input_api, output_api))
  results.extend(_CheckForIncludeGuards(input_api, output_api))
  results.extend(_CheckForWindowsLineEndings(input_api, output_api))
  results.extend(_CheckSingletonInHeaders(input_api, output_api))
  results.extend(_CheckPydepsNeedsUpdating(input_api, output_api))
  results.extend(_CheckJavaStyle(input_api, output_api))
  results.extend(_CheckIpcOwners(input_api, output_api))
  results.extend(_CheckSetNoParent(input_api, output_api))
  results.extend(_CheckUselessForwardDeclarations(input_api, output_api))
  results.extend(_CheckForRelativeIncludes(input_api, output_api))
  results.extend(_CheckForCcIncludes(input_api, output_api))
  results.extend(_CheckWATCHLISTS(input_api, output_api))
  results.extend(input_api.RunTests(
    input_api.canned_checks.CheckVPythonSpec(input_api, output_api)))
  results.extend(_CheckTranslationScreenshots(input_api, output_api))
  results.extend(_CheckCorrectProductNameInMessages(input_api, output_api))
  results.extend(_CheckBuildtoolsRevisionsAreInSync(input_api, output_api))
  results.extend(_CheckForTooLargeFiles(input_api, output_api))
  results.extend(_CheckPythonDevilInit(input_api, output_api))

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
  for f in input_api.AffectedSourceFiles(None):
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
  skipped_paths = ['third_party/sqlite/', 'third_party/abseil-cpp/']
  for f in input_api.AffectedFiles():
    if any([f.LocalPath().startswith(path) for path in skipped_paths]):
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
  for f in input_api.AffectedTestableFiles():
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
    # We also exclude *_message_generator.h headers as they use
    # include guards in a special, non-typical way.
    file_with_path = input_api.os_path.normpath(f.LocalPath())
    return (file_with_path.endswith('.h') and
            not file_with_path.endswith('_message_generator.h') and
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
                'Expected: %r\nFound:    %r' % (expected_guard, guard_name)))
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
  results.extend(_CheckFuzzTargets(input_api, output_api))
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
  results.extend(_AndroidSpecificOnCommitChecks(input_api, output_api))
  # Make sure the tree is 'open'.
  results.extend(input_api.canned_checks.CheckTreeIsOpen(
      input_api,
      output_api,
      json_url='http://chromium-status.appspot.com/current?format=json'))

  results.extend(
      input_api.canned_checks.CheckPatchFormatted(input_api, output_api))
  results.extend(input_api.canned_checks.CheckChangeHasBugField(
      input_api, output_api))
  results.extend(input_api.canned_checks.CheckChangeHasNoUnwantedTags(
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
        debug=False, defines={'_chromium': 1},
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
    if input_api.os_path.exists(sha1_path) and sha1_path not in removed_paths:
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
