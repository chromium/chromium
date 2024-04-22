// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains the default suppressions for LeakSanitizer.
// You can also pass additional suppressions via LSAN_OPTIONS:
// LSAN_OPTIONS=suppressions=/path/to/suppressions. Please refer to
// http://dev.chromium.org/developers/testing/leaksanitizer for more info.

#include "build/build_config.h"

#if defined(LEAK_SANITIZER)

// Please make sure the code below declares a single string variable
// kLSanDefaultSuppressions which contains LSan suppressions delimited by
// newlines. See http://dev.chromium.org/developers/testing/leaksanitizer
// for the instructions on writing suppressions.
char kLSanDefaultSuppressions[] =
    // Intentional leak used as sanity test for Valgrind/memcheck.
    "leak:base::ToolsSanityTest_MemoryLeak_Test::TestBody\n"

    // ================ Leaks in third-party code ================

    // False positives in libfontconfig. http://crbug.com/39050
    "leak:libfontconfig\n"
    "leak:libthird_party_fontconfig\n"
    // eglibc-2.19/string/strdup.c creates false positive leak errors because of
    // the same reason as crbug.com/39050. The leak error stack trace, when
    // unwind on malloc, includes a call to libfontconfig. But the default stack
    // trace is too short in leak sanitizer bot to make the libfontconfig
    // suppression works. http://crbug.com/605286
    "leak:__strdup\n"

    // Leaks in GL and Vulkan drivers and system libraries on Linux NVIDIA
    "leak:libGL.so\n"
    "leak:libGLX_nvidia.so\n"
    "leak:libnvidia-cbl.so\n"
    "leak:libnvidia-fatbinaryloader.so\n"
    "leak:libnvidia-glcore.so\n"
    "leak:libnvidia-rtcore.so\n"
    "leak:nvidia0\n"
    "leak:nvidiactl\n"
    "leak:libdbus-1.so\n"

    // XRandR has several one time leaks.
    "leak:libxrandr\n"

    // xrandr leak. http://crbug.com/119677
    "leak:XRRFindDisplay\n"

    // http://crbug.com/431213, http://crbug.com/416665
    "leak:gin/object_template_builder.h\n"
    "leak:gin/function_template.h\n"

    // Leaks in swrast_dri.so. http://crbug.com/540042
    "leak:swrast_dri.so\n"

    // Leak in glibc's gconv caused by fopen(..., "r,ccs=UNICODE")
    "leak:__gconv_lookup_cache\n"

    // Leak in libnssutil. crbug.com/1290634
    "leak:libnssutil3\n"

    // Suppress leaks from unknown third party modules. http://anglebug.com/6937
    "leak:<unknown module>\n"

    // Suppress leaks from temporary files. http://crbug.com/1433299
    "leak:(deleted)\n"

    // ================ Leaks in Chromium code ================
    // PLEASE DO NOT ADD SUPPRESSIONS FOR NEW LEAKS.
    // Instead, commits that introduce memory leaks should be reverted.
    // Suppressing the leak is acceptable in some cases when reverting is
    // impossible, i.e. when enabling leak detection for the first time for a
    // test target with pre-existing leaks.

    // v8 leaks caused by weak ref not call
    "leak:blink::DOMWrapperWorld::Create\n"
    "leak:blink::ScriptState::Create\n"

    // Crash keys are intentionally leaked.
    "leak:crash_reporter::(anonymous "
    "namespace)::CrashKeyBaseSupport::Allocate\n"

    // Suppress leaks in CreateCdmInstance. https://crbug.com/961062
    "leak:media::CdmAdapter::CreateCdmInstance\n"

#if BUILDFLAG(IS_CHROMEOS)
    // Suppress leak in FileStream. crbug.com/1263374
    "leak:chromeos::PipeReader::StartIO\n"
    // Supppress AnimationObserverToHideView leak. crbug.com/1261464
    "leak:ash::ShelfNavigationWidget::UpdateButtonVisibility\n"
    // Suppress AnimationSequence leak. crbug.com/1265031
    "leak:ash::LockStateController::StartPostLockAnimation\n"
    // Suppress leak in SurfaceDrawContext. crbug.com/1265033
    "leak:skgpu::v1::SurfaceDrawContext::drawGlyphRunList\n"
    // Suppress leak in BluetoothServerSocket. crbug.com/1278970
    "leak:nearby::chrome::BluetoothServerSocket::"
    "BluetoothServerSocket\n"
    // Suppress leak in NearbyConnectionBrokerImpl. crbug.com/1279578
    "leak:ash::secure_channel::NearbyConnectionBrokerImpl\n"
    // Suppress leak in NearbyEndpointFinderImpl. crbug.com/1288577
    "leak:ash::secure_channel::NearbyEndpointFinderImpl::~"
    "NearbyEndpointFinderImpl\n"
    // Suppress leak in DelayedCallbackGroup test. crbug.com/1279563
    "leak:DelayedCallbackGroup_TimeoutAndRun_Test\n"
#endif
#if BUILDFLAG(IS_MAC)
    // These are caused by the system, but not yet clear if they are false
    // positives or bugs in the Mac LSAN runtime. Suppress while investigating.
    // TODO(crbug.com/40223516): Remove these if/when fixed in macOS
    // or the runtime.
    "leak:_ensureAuxServiceAwareOfHostApp\n"
    "leak:cssmErrorString\n"
#endif

    // PLEASE READ ABOVE BEFORE ADDING NEW SUPPRESSIONS.

    // End of suppressions.
    ;  // Please keep this semicolon.

#endif  // LEAK_SANITIZER
