# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This is a list of files from the sysroot that are allowed to included from
# chromium code.
# Instructions:
# * Keep this list in sorted order.
# * If you need custom attributes (except exists), please explain them with a
#   comment.

# Note that this is filtered to only headers that are actually present.
# ie. If you add 'android/foo.h', it will silently ignore it on non-android
# platforms.

import dataclasses


@dataclasses.dataclass
class Header:
  path: str
  # Textual headers are, instead of being precompiled, textually included just
  # like you normally would with #include without modules.
  # A header should be textual if it is intended to be included multiple times
  # with different preprocessor configurations in a single build.
  # A good signal for this is if the header is missing include guards.
  # Note: Textual headers are a formal term used by clang.
  # See https://clang.llvm.org/docs/Modules.html
  textual: bool | None = None
  # Lazy headers are not added to the modulemap unless they are included by
  # another system header.
  lazy: bool = False
  # This specifies that the header only exists conditionally (eg. windows-only
  # headers). We could, instead of Header('foo', exists=is_win), write
  # *[Header('foo')] if is_win else [], but we provide this for simplicity's
  # sake.
  exists: bool = True
  # The name of the module for this header. Only useful for listing as an
  # export.
  module_name: str | None = None
  # A list of module names that things that #include this header should gain
  # access to. If you specify *, then it gains access to anything in its direct
  # dependencies.
  exports: list[str] = dataclasses.field(default_factory=lambda: ['*'])

  # If true, will force the header to create itself even if it wasn't in the
  # depfile.
  force: bool = False


# An allowed header is one that is here purely to add it to the allowlist of
# files to #include. It is not precompiled.
class AllowedHeader(Header):

  def __init__(self, path: str):
    super().__init__(path, force=True, lazy=True)


def headers(os):
  is_linux = os == 'linux'
  is_android = os == 'android'
  is_ios = os == 'ios'
  is_mac = os == 'mac'
  is_apple = os == 'mac' or os == 'ios'
  is_fuchsia = os == 'fuchsia'
  is_win = os == 'win'

  # Keep this list of headers alphabetically sorted, but comments should remain
  # attached to the entry under them, and blank lines should be preserved.

  return [
      Header('alloca.h'),
      # Include loop with sys/cdefs.h
      Header('android/api-level.h', textual=True),
      AllowedHeader('arpa/inet.h'),
      # We need posix_types_32.h to define __kernel_mode_t in the same TU.
      # This way it appears as an override rather than a second definition.
      Header('asm-generic/posix_types.h', textual=True, lazy=True),
      # Inherently textual
      Header('assert.h', textual=True),
      # avx512 headers are missing from clang modulemap.
      Header('avx512dqintrin.h', textual=True, lazy=True),
      # This isn't guarded, so it needs to be textual to prevent duplicate
      # definitions.
      Header('bits/mbstate_t.h', textual=False, lazy=True),
      # If this is textual it complains that pid_types isn't accessible from
      # sched.h
      Header('bits/timespec.h', textual=False, lazy=True),
      # We need to re-export std::mbstate_t in std.std_mbstate_t.
      Header('corecrt.h', exists=is_win, module_name='corecrt'),
      # We need to re-export wcsstr in a few places.
      Header('corecrt_wstring.h', exists=is_win, module_name='corecrt_wstring'),
      Header('ctype.h'),
      Header('cxxabi.h'),
      AllowedHeader('dirent.h'),
      AllowedHeader('dlfcn.h'),
      AllowedHeader('elf.h'),
      Header('endian.h'),
      AllowedHeader('fcntl.h'),
      Header('features.h'),
      Header('fenv.h'),
      AllowedHeader('grp.h'),
      AllowedHeader('libgen.h'),
      # See https://codebrowser.dev/glibc/glibc/sysdeps/unix/sysv/linux/bits/local_lim.h.html#56
      # if linux/limits.h is non-textual, then limits.h undefs the limits.h
      # defined in the linux/limits.h module.
      # Thus, limits.h exports an undef.
      # if it's textual, limits.h undefs something it defined itself.
      Header('limits.h', textual=True),
      AllowedHeader('link.h'),
      AllowedHeader('linux/futex.h'),
      # See above comment about limits.h
      Header('linux/limits.h', textual=True),
      AllowedHeader('linux/posix_types.h'),
      AllowedHeader('linux/random.h'),
      Header('linux/types.h'),
      Header('locale.h'),
      Header('malloc.h'),
      AllowedHeader('netdb.h'),
      AllowedHeader('netinet/in.h'),
      AllowedHeader('netinet/tcp.h'),
      # We need to re-export std::nothrow in std.new
      Header('new.h', exists=is_win, module_name='new_h'),
      AllowedHeader('poll.h'),
      Header('pthread.h'),
      Header('sal.h', exists=is_win),
      Header('sched.h'),
      AllowedHeader('semaphore.h'),
      Header('setjmp.h'),
      Header('signal.h'),
      # corecrt_wstring needs to be exported since it provides wcscmp
      Header('string.h', exists=is_win, exports=['*', 'corecrt_wstring']),
      Header('strings.h'),
      # In an include loop with features.h, but not on android
      AllowedHeader('sys/auxv.h'),
      Header('sys/cdefs.h', textual=not is_android),
      AllowedHeader('sys/eventfd.h'),
      AllowedHeader('sys/inotify.h'),
      AllowedHeader('sys/mman.h'),
      AllowedHeader('sys/prctl.h'),
      Header('sys/procfs.h'),
      AllowedHeader('sys/resource.h'),
      Header('sys/select.h'),
      AllowedHeader('sys/socket.h'),
      Header('sys/stat.h', exists=is_win),
      Header('sys/time.h'),
      AllowedHeader('sys/syscall.h'),
      AllowedHeader('sys/sysinfo.h'),
      AllowedHeader('sys/time.h'),
      AllowedHeader('sys/timerfd.h'),
      Header('sys/types.h'),
      Header('sys/ucontext.h'),
      AllowedHeader('sys/un.h'),
      Header('sys/user.h'),
      AllowedHeader('sys/utsname.h'),
      AllowedHeader('sys/wait.h'),
      AllowedHeader('syscall.h'),
      Header('time.h'),
      AllowedHeader('ucontext.h'),
      Header('unistd.h'),
      # We need to re-export std::exception in std.exception.exception and type
      # info.
      Header('vcruntime_exception.h',
             exists=is_win,
             module_name='vcruntime_exception'),
      # We need to re-export std::align_val_t in std.new.align_val_t.
      Header('vcruntime_new.h', exists=is_win, module_name='vcruntime_new'),
      # We need to re-export RTTI types (std::type_info, std::bad_cast, etc.)
      # in std.typeinfo.
      Header('vcruntime_typeinfo.h',
             exists=is_win,
             module_name='vcruntime_typeinfo',
             exports=['*', 'vcruntime_exception']),
      # include_next works differently with modules. This makes wchar.h and
      # mbstate_t.h an include loop.
      # This needs to be nontextual on windows, since otherwise it appears under
      # cwchar.
      Header('wchar.h', textual=not is_win),
      Header('winapifamily.h', exists=is_win),
  ]
