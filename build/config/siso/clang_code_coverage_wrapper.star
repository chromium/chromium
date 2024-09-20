# -*- bazel-starlark -*-
# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Siso config version of clang_code_coverage_wrapper.py"""
# LINT.IfChange

load("@builtin//struct.star", "module")

# Logics are copied from build/toolchain/clang_code_coverage_wrapper.py
# in ordre to strip coverage flags without process invocation.
# This is neceesary for Siso to send clang command to RBE without the wrapper and instrument file.

# Flags used to enable coverage instrumentation.
# Flags should be listed in the same order that they are added in
# build/config/coverage/BUILD.gn
_COVERAGE_FLAGS = [
    "-fprofile-instr-generate",
    "-fcoverage-mapping",
    "-mllvm",
    "-runtime-counter-relocation=true",
    # Following experimental flags remove unused header functions from the
    # coverage mapping data embedded in the test binaries, and the reduction
    # of binary size enables building Chrome's large unit test targets on
    # MacOS. Please refer to crbug.com/796290 for more details.
    "-mllvm",
    "-limited-coverage-experimental=true",
]

# Files that should not be built with coverage flags by default.
_DEFAULT_COVERAGE_EXCLUSION_LIST = [
    # These files caused perf regressions, resulting in time-outs on some bots.
    # TODO(https://crbug.com/356570413): Remove when the bug is fixed.
    "../../base/allocator/partition_allocator/src/partition_alloc/address_pool_manager_bitmap.cc",
    "../../base/allocator/partition_allocator/src/partition_alloc/address_pool_manager.cc",
    "../../base/allocator/partition_allocator/src/partition_alloc/address_pool_manager_unittest.cc",
    "../../base/allocator/partition_allocator/src/partition_alloc/address_space_randomization.cc",
    "../../base/allocator/partition_allocator/src/partition_alloc/address_space_randomization_unittest.cc",
    "../../base/allocator/partition_allocator/src/partition_alloc/allocation_guard.cc",
    "../../base/allocator/partition_allocator/src/partition_alloc/compressed_pointer.cc",
    "../../base/allocator/partition_allocator/src/partition_alloc/compressed_pointer_unittest.cc",
    "../../base/allocator/partition_allocator/src/partition_alloc/dangling_raw_ptr_checks.cc",
    "../../base/allocator/partition_allocator/src/partition_alloc/extended_api.cc",
    "../../base/allocator/partition_allocator/src/partition_alloc/freeslot_bitmap_unittest.cc",
    "../../base/allocator/partition_allocator/src/partition_alloc/gwp_asan_support.cc",
    "../../base/allocator/partition_allocator/src/partition_alloc/hardening_unittest.cc",
    "../../base/allocator/partition_allocator/src/partition_alloc/internal_allocator.cc",
    "../../base/allocator/partition_allocator/src/partition_alloc/lightweight_quarantine.cc",
    "../../base/allocator/partition_allocator/src/partition_alloc/lightweight_quarantine_unittest.cc",
    "../../base/allocator/partition_allocator/src/partition_alloc/memory_reclaimer.cc",
    "../../base/allocator/partition_allocator/src/partition_alloc/memory_reclaimer_unittest.cc",
    "../../base/allocator/partition_allocator/src/partition_alloc/oom_callback.cc",
    "../../base/allocator/partition_allocator/src/partition_alloc/oom.cc",
    "../../base/allocator/partition_allocator/src/partition_alloc/page_allocator.cc",
    "../../base/allocator/partition_allocator/src/partition_alloc/page_allocator_internals_posix.cc",
    "../../base/allocator/partition_allocator/src/partition_alloc/page_allocator_unittest.cc",
    "../../base/allocator/partition_allocator/src/partition_alloc/partition_address_space.cc",
    "../../base/allocator/partition_allocator/src/partition_alloc/partition_alloc_base/apple/mach_logging.cc",
    "../../base/allocator/partition_allocator/src/partition_alloc/partition_alloc_base/bits_pa_unittest.cc",
    "../../base/allocator/partition_allocator/src/partition_alloc/partition_alloc_base/check.cc",
    "../../base/allocator/partition_allocator/src/partition_alloc/partition_alloc_base/component_export_pa_unittest.cc",
    "../../base/allocator/partition_allocator/src/partition_alloc/partition_alloc_base/cpu.cc",
    "../../base/allocator/partition_allocator/src/partition_alloc/partition_alloc_base/cpu_pa_unittest.cc",
    "../../base/allocator/partition_allocator/src/partition_alloc/partition_alloc_base/debug/alias.cc",
    "../../base/allocator/partition_allocator/src/partition_alloc/partition_alloc_base/debug/proc_maps_linux.cc",
    "../../base/allocator/partition_allocator/src/partition_alloc/partition_alloc_base/debug/stack_trace_android.cc",
    "../../base/allocator/partition_allocator/src/partition_alloc/partition_alloc_base/debug/stack_trace.cc",
    "../../base/allocator/partition_allocator/src/partition_alloc/partition_alloc_base/debug/stack_trace_linux.cc",
    "../../base/allocator/partition_allocator/src/partition_alloc/partition_alloc_base/debug/stack_trace_mac.cc",
    "../../base/allocator/partition_allocator/src/partition_alloc/partition_alloc_base/debug/stack_trace_posix.cc",
    "../../base/allocator/partition_allocator/src/partition_alloc/partition_alloc_base/debug/stack_trace_win.cc",
    "../../base/allocator/partition_allocator/src/partition_alloc/partition_alloc_base/files/file_path.cc",
    "../../base/allocator/partition_allocator/src/partition_alloc/partition_alloc_base/files/file_path_pa_unittest.cc",
    "../../base/allocator/partition_allocator/src/partition_alloc/partition_alloc_base/files/file_util_posix.cc",
    "../../base/allocator/partition_allocator/src/partition_alloc/partition_alloc_base/fuchsia/fuchsia_logging.cc",
    "../../base/allocator/partition_allocator/src/partition_alloc/partition_alloc_base/fuchsia/fuchsia_logging_pa_unittest.cc",
    "../../base/allocator/partition_allocator/src/partition_alloc/partition_alloc_base/logging.cc",
    "../../base/allocator/partition_allocator/src/partition_alloc/partition_alloc_base/logging_pa_unittest.cc",
    "../../base/allocator/partition_allocator/src/partition_alloc/partition_alloc_base/log_message.cc",
    "../../base/allocator/partition_allocator/src/partition_alloc/partition_alloc_base/memory/page_size_posix.cc",
    "../../base/allocator/partition_allocator/src/partition_alloc/partition_alloc_base/memory/page_size_win.cc",
    "../../base/allocator/partition_allocator/src/partition_alloc/partition_alloc_base/memory/ref_counted.cc",
    "../../base/allocator/partition_allocator/src/partition_alloc/partition_alloc_base/native_library.cc",
    "../../base/allocator/partition_allocator/src/partition_alloc/partition_alloc_base/native_library_pa_unittest.cc",
    "../../base/allocator/partition_allocator/src/partition_alloc/partition_alloc_base/native_library_posix.cc",
    "../../base/allocator/partition_allocator/src/partition_alloc/partition_alloc_base/no_destructor_pa_unittest.cc",
    "../../base/allocator/partition_allocator/src/partition_alloc/partition_alloc_base/posix/safe_strerror.cc",
    "../../base/allocator/partition_allocator/src/partition_alloc/partition_alloc_base/process/process_handle_posix.cc",
    "../../base/allocator/partition_allocator/src/partition_alloc/partition_alloc_base/process/process_handle_win.cc",
    "../../base/allocator/partition_allocator/src/partition_alloc/partition_alloc_base/rand_util.cc",
    "../../base/allocator/partition_allocator/src/partition_alloc/partition_alloc_base/rand_util_fuchsia.cc",
    "../../base/allocator/partition_allocator/src/partition_alloc/partition_alloc_base/rand_util_pa_unittest.cc",
    "../../base/allocator/partition_allocator/src/partition_alloc/partition_alloc_base/rand_util_posix.cc",
    "../../base/allocator/partition_allocator/src/partition_alloc/partition_alloc_base/rand_util_win.cc",
    "../../base/allocator/partition_allocator/src/partition_alloc/partition_alloc_base/scoped_clear_last_error_pa_unittest.cc",
    "../../base/allocator/partition_allocator/src/partition_alloc/partition_alloc_base/scoped_clear_last_error_win.cc",
    "../../base/allocator/partition_allocator/src/partition_alloc/partition_alloc_base/strings/cstring_builder.cc",
    "../../base/allocator/partition_allocator/src/partition_alloc/partition_alloc_base/strings/cstring_builder_pa_unittest.cc",
    "../../base/allocator/partition_allocator/src/partition_alloc/partition_alloc_base/strings/safe_sprintf.cc",
    "../../base/allocator/partition_allocator/src/partition_alloc/partition_alloc_base/strings/safe_sprintf_pa_unittest.cc",
    "../../base/allocator/partition_allocator/src/partition_alloc/partition_alloc_base/strings/stringprintf.cc",
    "../../base/allocator/partition_allocator/src/partition_alloc/partition_alloc_base/strings/stringprintf_pa_unittest.cc",
    "../../base/allocator/partition_allocator/src/partition_alloc/partition_alloc_base/strings/string_util.cc",
    "../../base/allocator/partition_allocator/src/partition_alloc/partition_alloc_base/strings/string_util_pa_unittest.cc",
    "../../base/allocator/partition_allocator/src/partition_alloc/partition_alloc_base/thread_annotations_pa_unittest.cc",
    "../../base/allocator/partition_allocator/src/partition_alloc/partition_alloc_base/threading/platform_thread_android_for_testing.cc",
    "../../base/allocator/partition_allocator/src/partition_alloc/partition_alloc_base/threading/platform_thread.cc",
    "../../base/allocator/partition_allocator/src/partition_alloc/partition_alloc_base/threading/platform_thread_fuchsia_for_testing.cc",
    "../../base/allocator/partition_allocator/src/partition_alloc/partition_alloc_base/threading/platform_thread_linux_for_testing.cc",
    "../../base/allocator/partition_allocator/src/partition_alloc/partition_alloc_base/threading/platform_thread_posix.cc",
    "../../base/allocator/partition_allocator/src/partition_alloc/partition_alloc_base/threading/platform_thread_posix_for_testing.cc",
    "../../base/allocator/partition_allocator/src/partition_alloc/partition_alloc_base/threading/platform_thread_win.cc",
    "../../base/allocator/partition_allocator/src/partition_alloc/partition_alloc_base/threading/platform_thread_win_for_testing.cc",
    "../../base/allocator/partition_allocator/src/partition_alloc/partition_alloc_base/time/time.cc",
    "../../base/allocator/partition_allocator/src/partition_alloc/partition_alloc_base/time/time_conversion_posix.cc",
    "../../base/allocator/partition_allocator/src/partition_alloc/partition_alloc_base/time/time_fuchsia.cc",
    "../../base/allocator/partition_allocator/src/partition_alloc/partition_alloc_base/time/time_now_posix.cc",
    "../../base/allocator/partition_allocator/src/partition_alloc/partition_alloc_base/time/time_override.cc",
    "../../base/allocator/partition_allocator/src/partition_alloc/partition_alloc_base/time/time_win.cc",
    "../../base/allocator/partition_allocator/src/partition_alloc/partition_alloc.cc",
    "../../base/allocator/partition_allocator/src/partition_alloc/partition_alloc_hooks.cc",
    "../../base/allocator/partition_allocator/src/partition_alloc/partition_alloc_perftest.cc",
    "../../base/allocator/partition_allocator/src/partition_alloc/partition_alloc_unittest.cc",
    "../../base/allocator/partition_allocator/src/partition_alloc/partition_bucket.cc",
    "../../base/allocator/partition_allocator/src/partition_alloc/partition_dcheck_helper.cc",
    "../../base/allocator/partition_allocator/src/partition_alloc/partition_freelist_entry.cc",
    "../../base/allocator/partition_allocator/src/partition_alloc/partition_lock_perftest.cc",
    "../../base/allocator/partition_allocator/src/partition_alloc/partition_lock_unittest.cc",
    "../../base/allocator/partition_allocator/src/partition_alloc/partition_oom.cc",
    "../../base/allocator/partition_allocator/src/partition_alloc/partition_page.cc",
    "../../base/allocator/partition_allocator/src/partition_alloc/partition_root.cc",
    "../../base/allocator/partition_allocator/src/partition_alloc/partition_stats.cc",
    "../../base/allocator/partition_allocator/src/partition_alloc/partition_tls_win.cc",
    "../../base/allocator/partition_allocator/src/partition_alloc/pointers/empty.cc",
    "../../base/allocator/partition_allocator/src/partition_alloc/pointers/instance_tracer.cc",
    "../../base/allocator/partition_allocator/src/partition_alloc/pointers/raw_ptr_asan_unowned_impl.cc",
    "../../base/allocator/partition_allocator/src/partition_alloc/pointers/raw_ptr_backup_ref_impl.cc",
    "../../base/allocator/partition_allocator/src/partition_alloc/pointers/raw_ptr_hookable_impl.cc",
    "../../base/allocator/partition_allocator/src/partition_alloc/pointers/raw_ptr_unittest.cc",
    "../../base/allocator/partition_allocator/src/partition_alloc/pointers/raw_ref_unittest.cc",
    "../../base/allocator/partition_allocator/src/partition_alloc/random.cc",
    "../../base/allocator/partition_allocator/src/partition_alloc/reservation_offset_table.cc",
    "../../base/allocator/partition_allocator/src/partition_alloc/reverse_bytes_unittest.cc",
    "../../base/allocator/partition_allocator/src/partition_alloc/shim/allocator_shim_android.cc",
    "../../base/allocator/partition_allocator/src/partition_alloc/shim/allocator_shim_apple.cc",
    "../../base/allocator/partition_allocator/src/partition_alloc/shim/allocator_shim.cc",
    "../../base/allocator/partition_allocator/src/partition_alloc/shim/allocator_shim_default_dispatch_to_apple_zoned_malloc.cc",
    "../../base/allocator/partition_allocator/src/partition_alloc/shim/allocator_shim_default_dispatch_to_glibc.cc",
    "../../base/allocator/partition_allocator/src/partition_alloc/shim/allocator_shim_default_dispatch_to_linker_wrapped_symbols.cc",
    "../../base/allocator/partition_allocator/src/partition_alloc/shim/allocator_shim_default_dispatch_to_partition_alloc.cc",
    "../../base/allocator/partition_allocator/src/partition_alloc/shim/allocator_shim_default_dispatch_to_partition_alloc_unittest.cc",
    "../../base/allocator/partition_allocator/src/partition_alloc/shim/allocator_shim_default_dispatch_to_partition_alloc_with_advanced_checks.cc",
    "../../base/allocator/partition_allocator/src/partition_alloc/shim/allocator_shim_default_dispatch_to_winheap.cc",
    "../../base/allocator/partition_allocator/src/partition_alloc/shim/allocator_shim_dispatch_to_noop_on_free.cc",
    "../../base/allocator/partition_allocator/src/partition_alloc/shim/allocator_shim_functions_win_component.cc",
    "../../base/allocator/partition_allocator/src/partition_alloc/shim/allocator_shim_unittest.cc",
    "../../base/allocator/partition_allocator/src/partition_alloc/shim/allocator_shim_win_component.cc",
    "../../base/allocator/partition_allocator/src/partition_alloc/shim/allocator_shim_win_static.cc",
    "../../base/allocator/partition_allocator/src/partition_alloc/shim/empty.cc",
    "../../base/allocator/partition_allocator/src/partition_alloc/shim/malloc_zone_functions_apple.cc",
    "../../base/allocator/partition_allocator/src/partition_alloc/shim/malloc_zone_functions_apple_unittest.cc",
    "../../base/allocator/partition_allocator/src/partition_alloc/shim/winheap_stubs_win.cc",
    "../../base/allocator/partition_allocator/src/partition_alloc/shim/winheap_stubs_win_unittest.cc",
    "../../base/allocator/partition_allocator/src/partition_alloc/slot_start_unittest.cc",
    "../../base/allocator/partition_allocator/src/partition_alloc/spinning_mutex.cc",
    "../../base/allocator/partition_allocator/src/partition_alloc/stack/asm/arm64/push_registers_asm.cc",
    "../../base/allocator/partition_allocator/src/partition_alloc/stack/asm/arm/push_registers_asm.cc",
    "../../base/allocator/partition_allocator/src/partition_alloc/stack/asm/riscv64/push_registers_asm.cc",
    "../../base/allocator/partition_allocator/src/partition_alloc/stack/asm/x64/push_registers_asm.cc",
    "../../base/allocator/partition_allocator/src/partition_alloc/stack/asm/x86/push_registers_asm.cc",
    "../../base/allocator/partition_allocator/src/partition_alloc/stack/stack.cc",
    "../../base/allocator/partition_allocator/src/partition_alloc/stack/stack_unittest.cc",
    "../../base/allocator/partition_allocator/src/partition_alloc/tagging.cc",
    "../../base/allocator/partition_allocator/src/partition_alloc/tagging_unittest.cc",
    "../../base/allocator/partition_allocator/src/partition_alloc/thread_cache.cc",
    "../../base/allocator/partition_allocator/src/partition_alloc/thread_cache_unittest.cc",
    "../../base/allocator/partition_allocator/src/partition_alloc/thread_isolation/pkey.cc",
    "../../base/allocator/partition_allocator/src/partition_alloc/thread_isolation/pkey_unittest.cc",
    "../../base/allocator/partition_allocator/src/partition_alloc/thread_isolation/thread_isolation.cc",
]

# Map of exclusion lists indexed by target OS.
# If no target OS is defined, or one is defined that doesn't have a specific
# entry, use _DEFAULT_COVERAGE_EXCLUSION_LIST.
_COVERAGE_EXCLUSION_LIST_MAP = {
    "android": [
        # This file caused webview native library failed on arm64.
        "../../device/gamepad/dualshock4_controller.cc",
    ] + _DEFAULT_COVERAGE_EXCLUSION_LIST,
    "fuchsia": [
        # TODO(crbug.com/40167659): These files caused clang to crash while
        # compiling them.
        "../../third_party/skia/src/core/SkOpts.cpp",
        "../../third_party/skia/src/opts/SkOpts_hsw.cpp",
        "../../third_party/skia/third_party/skcms/skcms.cc",
    ] + _DEFAULT_COVERAGE_EXCLUSION_LIST,
    "linux": [
        # These files caused a static initializer to be generated, which
        # shouldn't.
        # TODO(crbug.com/41474559): Remove when the bug is fixed.
        "../../chrome/browser/media/router/providers/cast/cast_internal_message_util.cc",  #pylint: disable=line-too-long
        "../../components/media_router/common/providers/cast/channel/cast_channel_enum.cc",  #pylint: disable=line-too-long
        "../../components/media_router/common/providers/cast/channel/cast_message_util.cc",  #pylint: disable=line-too-long
        "../../components/media_router/common/providers/cast/cast_media_source.cc",  #pylint: disable=line-too-long
        "../../ui/events/keycodes/dom/keycode_converter.cc",
    ] + _DEFAULT_COVERAGE_EXCLUSION_LIST,
    "chromeos": [
        # These files caused clang to crash while compiling them. They are
        # excluded pending an investigation into the underlying compiler bug.
        "../../third_party/webrtc/p2p/base/p2p_transport_channel.cc",
        "../../third_party/icu/source/common/uts46.cpp",
        "../../third_party/icu/source/common/ucnvmbcs.cpp",
        "../../base/android/android_image_reader_compat.cc",
    ] + _DEFAULT_COVERAGE_EXCLUSION_LIST,
}

# Map of force lists indexed by target OS.
_COVERAGE_FORCE_LIST_MAP = {
    # clang_profiling.cc refers to the symbol `__llvm_profile_dump` from the
    # profiling runtime. In a partial coverage build, it is possible for a
    # binary to include clang_profiling.cc but have no instrumented files, thus
    # causing an unresolved symbol error because the profiling runtime will not
    # be linked in. Therefore we force coverage for this file to ensure that
    # any target that includes it will also get the profiling runtime.
    "win": [r"..\..\base\test\clang_profiling.cc"],
    # TODO(crbug.com/40154378) We're seeing runtime LLVM errors in mac-rel when
    # no files are changed, so we suspect that this is similar to the other
    # problem with clang_profiling.cc on Windows. The TODO here is to force
    # coverage for this specific file on ALL platforms, if it turns out to fix
    # this issue on Mac as well. It's the only file that directly calls
    # `__llvm_profile_dump` so it warrants some special treatment.
    "mac": ["../../base/test/clang_profiling.cc"],
}

def _remove_flags_from_command(command):
    # We need to remove the coverage flags for this file, but we only want to
    # remove them if we see the exact sequence defined in _COVERAGE_FLAGS.
    # That ensures that we only remove the flags added by GN when
    # "use_clang_coverage" is true. Otherwise, we would remove flags set by
    # other parts of the build system.
    start_flag = _COVERAGE_FLAGS[0]
    num_flags = len(_COVERAGE_FLAGS)
    start_idx = 0

    def _start_flag_idx(cmd, start_idx):
        for i in range(start_idx, len(cmd)):
            if cmd[i] == start_flag:
                return i

    # Workaround to emulate while loop in Starlark.
    for _ in range(0, len(command)):
        idx = _start_flag_idx(command, start_idx)
        if not idx:
            # Coverage flags are not included anymore.
            return command
        if command[idx:idx + num_flags] == _COVERAGE_FLAGS:
            # Starlark doesn't have `del`.
            command = command[:idx] + command[idx + num_flags:]

            # There can be multiple sets of _COVERAGE_FLAGS. All of these need to be
            # removed.
            start_idx = idx
        else:
            start_idx = idx + 1
    return command

def __run(ctx, args):
    """Runs the main logic of clang_code_coverage_wrapper.

      This is slightly different from the main function of clang_code_coverage_wrapper.py
      because starlark can't use Python's standard libraries.
    """
    # We need to remove the coverage flags for this file, but we only want to
    # remove them if we see the exact sequence defined in _COVERAGE_FLAGS.
    # That ensures that we only remove the flags added by GN when
    # "use_clang_coverage" is true. Otherwise, we would remove flags set by
    # other parts of the build system.

    if len(args) == 0:
        return args
    if not args[0].endswith("python3") and not args[0].endswith("python3.exe"):
        return args

    has_coveage_wrapper = False
    instrument_file = None
    compile_command_pos = None
    target_os = None
    source_flag = "-c"
    source_flag_index = None
    for i, arg in enumerate(args):
        if i == 0:
            continue
        if arg == "../../build/toolchain/clang_code_coverage_wrapper.py":
            has_coveage_wrapper = True
            continue
        if arg.startswith("--files-to-instrument="):
            instrument_file = arg.removeprefix("--files-to-instrument=")
            continue
        if arg.startswith("--target-os="):
            target_os = arg.removeprefix("--target-os=")
            if target_os == "win":
                source_flag = "/c"
            continue
        if not compile_command_pos and not args[i].startswith("-") and "clang" in args[i]:
            compile_command_pos = i
            continue
        if args[i] == source_flag:
            # The command is assumed to use Clang as the compiler, and the path to the
            # source file is behind the -c argument, and the path to the source path is
            # relative to the root build directory. For example:
            # clang++ -fvisibility=hidden -c ../../base/files/file_path.cc -o \
            #   obj/base/base/file_path.o
            # On Windows, clang-cl.exe uses /c instead of -c.
            source_flag_index = i
            continue

    if not has_coveage_wrapper or not compile_command_pos:
        print("this is not clang coverage command. %s" % str(args))
        return args

    compile_command = args[compile_command_pos:]

    if not source_flag_index:
        fail("%s argument is not found in the compile command. %s" % (source_flag, str(args)))

    if source_flag_index + 1 >= len(args):
        fail("Source file to be compiled is missing from the command.")

    # On Windows, filesystem paths should use '\', but GN creates build commands
    # that use '/'.
    # The original logic in clang_code_coverage_wrapper.py uses
    # os.path.normpath() to ensure to ensure that the path uses the correct
    # separator for the current platform. i.e. '\' on Windows and '/' otherwise
    # Siso's ctx.fs.canonpath() ensures '/' on all platforms, instead.
    # TODO: Consdier coverting the paths in instrument file and hardcoded lists
    # only once at initialization if it improves performance.

    compile_source_file = ctx.fs.canonpath(args[source_flag_index + 1])

    extension = compile_source_file.rsplit(".", 1)[1]
    if not extension in ["c", "cc", "cpp", "cxx", "m", "mm", "S"]:
        fail("Invalid source file %s found. extension=%s" % (compile_source_file, extension))

    exclusion_list = _COVERAGE_EXCLUSION_LIST_MAP.get(
        target_os,
        _DEFAULT_COVERAGE_EXCLUSION_LIST,
    )
    exclusion_list = [ctx.fs.canonpath(f) for f in exclusion_list]
    force_list = _COVERAGE_FORCE_LIST_MAP.get(target_os, [])
    force_list = [ctx.fs.canonpath(f) for f in force_list]

    files_to_instrument = []
    if instrument_file:
        files_to_instrument = str(ctx.fs.read(ctx.fs.canonpath(instrument_file))).splitlines()

        # strip() is for removing '\r' on Windows.
        files_to_instrument = [ctx.fs.canonpath(f).strip() for f in files_to_instrument]

    should_remove_flags = False
    if compile_source_file not in force_list:
        if compile_source_file in exclusion_list:
            should_remove_flags = True
        elif instrument_file and compile_source_file not in files_to_instrument:
            should_remove_flags = True

    if should_remove_flags:
        return _remove_flags_from_command(compile_command)
    print("Keeping code coverage flags for %s" % compile_source_file)
    return compile_command

clang_code_coverage_wrapper = module(
    "clang_code_coverage_wrapper",
    run = __run,
)

# LINT.ThenChange(/build/toolchain/clang_code_coverage_wrapper.py)
