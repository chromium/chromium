// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains the default suppressions for ThreadSanitizer.
// You can also pass additional suppressions via TSAN_OPTIONS:
// TSAN_OPTIONS=suppressions=/path/to/suppressions. Please refer to
// http://dev.chromium.org/developers/testing/threadsanitizer-tsan-v2
// for more info.

#if defined(THREAD_SANITIZER)

// Please make sure the code below declares a single string variable
// kTSanDefaultSuppressions contains TSan suppressions delimited by newlines.
// See http://dev.chromium.org/developers/testing/threadsanitizer-tsan-v2
// for the instructions on writing suppressions.
char kTSanDefaultSuppressions[] =
    // False positives in libdbus.so, libdconfsettings.so, libflashplayer.so,
    // libgio.so, libglib.so, libgobject.so, and libfontconfig.so.1.
    // Since we don't instrument them, we cannot reason about the
    // synchronization in them.
    "race:libdbus*.so\n"
    "race:libdconfsettings*.so\n"
    "race:libflashplayer.so\n"
    "race:libgio*.so\n"
    "race:libglib*.so\n"
    "race:libgobject*.so\n"
    "race:libfontconfig.so.1\n"

    // Intentional race in ToolsSanityTest.DataRace in base_unittests.
    "race:base/tools_sanity_unittest.cc\n"

    // Data race on WatchdogCounter [test-only].
    "race:base/threading/watchdog_unittest.cc\n"

    // Data race caused by swapping out the network change notifier with a mock
    // [test-only]. http://crbug.com/927330.
    "race:content/browser/net_info_browsertest.cc\n"

    // http://crbug.com/120808
    "race:base/threading/watchdog.cc\n"

    // http://crbug.com/157586
    "race:third_party/libvpx/source/libvpx/vp8/decoder/threading.c\n"

    // http://crbug.com/244856
    "race:libpulsecommon*.so\n"

    // http://crbug.com/268924
    "race:base::g_power_monitor\n"
    "race:base::PowerMonitor::PowerMonitor\n"
    "race:base::PowerMonitor::AddObserver\n"
    "race:base::PowerMonitor::RemoveObserver\n"
    "race:base::PowerMonitor::IsOnBatteryPower\n"

    // http://crbug.com/308590
    "race:CustomThreadWatcher::~CustomThreadWatcher\n"

    // http://crbug.com/476529
    "deadlock:cc::VideoLayerImpl::WillDraw\n"

    // http://crbug.com/328826
    "race:skia::(anonymous namespace)::g_pixel_geometry\n"

    // http://crbug.com/328868
    "race:PR_Lock\n"

    // False positive in libc's tzset_internal, http://crbug.com/379738.
    "race:tzset_internal\n"

    // http://crbug.com/380554
    "deadlock:g_type_add_interface_static\n"

    // http://crbug.com/397022
    "deadlock:"
    "base::trace_event::TraceEventTestFixture_ThreadOnceBlocking_Test::"
    "TestBody\n"

    // http://crbug.com/415472
    "deadlock:base::trace_event::TraceLog::GetCategoryGroupEnabled\n"

    // Lock inversion in third party code, won't fix.
    // https://crbug.com/455638
    "deadlock:dbus::Bus::ShutdownAndBlock\n"

    // https://crbug.com/459429
    "race:randomnessPid\n"

    // http://crbug.com/633145
    "race:third_party/libjpeg_turbo/simd/jsimd_x86_64.c\n"

    // http://crbug.com/691029
    "deadlock:libGLX.so*\n"

    //  http://crbug.com/973947
    "deadlock:libnvidia-glsi.so*\n"

    // http://crbug.com/695929
    "race:base::i18n::IsRTL\n"
    "race:base::i18n::SetICUDefaultLocale\n"

    // http://crbug.com/927330
    "race:net::(anonymous namespace)::g_network_change_notifier\n"

    // https://crbug.com/977085
    "race:vp3_update_thread_context\n"

    // Benign data race in libjpeg-turbo, won't fix
    // (https://github.com/libjpeg-turbo/libjpeg-turbo/issues/87).
    // https://crbug.com/1056011
    "race:third_party/libjpeg_turbo/simd/x86_64/jsimd.c\n"

    // https://crbug.com/1158622
    "race:absl::synchronization_internal::Waiter::Post\n"

    // Harmless data races, see WTF::StringImpl::Release code comments.
    "race:scoped_refptr<WTF::StringImpl>::AddRef\n"
    "race:scoped_refptr<WTF::StringImpl>::Release\n"

    // Harmless data race in ipcz block allocation. See comments in
    // ipcz::BlockAllocator::Allocate().
    "race:ipcz::BlockAllocator::Allocate\n"

    // https://crbug.com/1405439
    "race:perfetto::perfetto_track_event::internal::g_category_state_storage\n"
    "race:perfetto::DataSource*::static_state_\n"
    "race:perfetto::Tracing::ResetForTesting\n"

    // End of suppressions.
    ;  // Please keep this semicolon.

#endif  // THREAD_SANITIZER
