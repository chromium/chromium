# Malloc interposition / interception

## Platforms and how to interpose / intercept

| Platform | The way of interposition / interception |
| --- | --- |
| Android | [Link-time rewriting](#link_time-rewriting) (\*1) |
| ChromeOS, GNU/Linux, etc. (\*2) | [Weak symbol overriding](#weak-symbol-overriding-load_time-interposition) |
| iOS, macOS (Apple OSes) | [Malloc zone](#malloc-zone-runtime-interception) |
| Windows | _to be written_ |

(\*1) Android P and newer versions support [malloc hooks](https://android.googlesource.com/platform/bionic/+/master/libc/malloc_hooks/README.md), but there is no way to enable it on end-user environments as it requires (a) setting a special system property, or (b) setting a special environment variable. Thus, PartitionAlloc doesn't use these malloc hooks.

(\*2) Applicable to all platforms that support ELF format.

## Link-time rewriting

GNU ld compatible linkers support `--wrap <symbol>` option.
The linkers replace unresolved references to `<symbol>` with `__wrap_<symbol>` and unresolved references to `__real_<symbol>` with `<symbol>`.

PartitionAlloc uses this option to make client code call [PartitionAlloc's malloc](https://source.chromium.org/chromium/chromium/src/+/main:base/allocator/partition_allocator/src/partition_alloc/shim/allocator_shim_override_linker_wrapped_symbols.h?q=func:%5E__wrap_malloc$%20case:yes&ss=chromium) instead of libc's `malloc`.
This link-time rewriting is effective only to the object files to which a linker applies the `--wrap <symbol>` option.
So, all libraries that are directly or indirectly loaded from an executable should be linked with the option.
Otherwise, the object files not linked with the option will use libc's malloc.
In particular, OS shared libraries are not linked with the option.
On Android, this means that userspace OpenGL drivers or other platform libraries (including the native side of its UI framework) etc. do not use PartitionAlloc, leaving a non-trivial part of the heap off-limit.

The list of the target symbols to which PartitionAlloc applies this option is [here](https://source.chromium.org/chromium/chromium/src/+/main:base/allocator/partition_allocator/src/partition_alloc/BUILD.gn?q=wrap_malloc_symbols&ss=chromium).

Technically this way of the interposition can be applied to all platforms, however, it's hard to apply the linker option to all object files, and also hard to detect object files to which the linker option is not applied.
So, this way is considered fragile, and applied only to Android where no other option is available.

## Weak symbol overriding (load-time interposition)

`malloc` function family is declared as [weak symbols](https://en.wikipedia.org/wiki/Weak_symbol) so that they can be overridden by user-defined functions which are strong symbols.
So there is no difficulty there.

PartitionAlloc exports `malloc` family's functions as strong symbols.
These functions are loaded as `malloc` family, and the [GOT](https://en.wikipedia.org/wiki/Global_Offset_Table) stores them.
So all allocations/deallocations since start-up time will be handled by PartitionAlloc.
For example, `strdup` in the standard C library internally calls `malloc`, and the function look-up for `malloc` finds the PartitionAlloc's malloc.
Everything works as expected.

## Malloc zone (runtime interception)

iOS and macOS support [malloc zones](https://developer.apple.com/library/archive/documentation/Performance/Conceptual/ManagingMemory/Articles/MemoryAlloc.html#//apple_ref/doc/uid/20001881-99932-CJBEAJHF).
Calls to `malloc` are routed to the default malloc zone which is the first zone in the malloc zone list.
Calls to `free` are routed to the malloc zone that claims that the zone owns the memory chunk pointed to by the given pointer.

PartitionAlloc registers a new malloc zone and makes it the new default malloc zone.
From this point of time, calls to `malloc` will be routed to the new default malloc zone of PartitionAlloc, and PartitionAlloc allocates memory.
Allocations prior to this point of time were handled by the existing default malloc zone of the operating system, so calls to `free` need to be dispatched to either PartitionAlloc or the operating system accordingly.

It's tricky to make a malloc zone the new default malloc zone because there is no such an API to do it.
There are only two APIs that control malloc zone registration: `malloc_zone_register` and `malloc_zone_unregister`.

- `malloc_zone_regisger` adds a new malloc zone at the end of the malloc zone list.
- `malloc_zone_unregister` swaps the malloc zone being unregistered with the last zone in the malloc zone list (i.e. moves the malloc zone being unregistered to the last), and then removes the malloc zone to be unregistered.

Given these two APIs, `malloc_zone_unregister` is the only way to promote our own malloc zone to the new default malloc zone.
By unregistering the existing default malloc zone (the first malloc zone), the last malloc zone becomes the new default malloc zone.
Note that the existing default malloc zone must be re-registered because allocations made prior to the default malloc zone replacement need to be deallocated by the proper owner malloc zone, which is the existing default malloc zone.

The above way of the default malloc zone replacement works, but there is a race issue when running multiple threads.
`malloc_zone_register` and `malloc_zone_unregister` are thread-safe and work atomically, but they are two separate APIs and there is no way to run both APIs atomically.
In addition, zone lookup isn’t synchronized. So, when the existing default malloc zone is unregistered first and then re-registered, there exists a moment that the existing default malloc zone is not in the malloc zone list.
A call to `free` for an allocation by the existing default malloc zone will end up with a crash during that moment.
This problem can be worked around by registering a copy of the existing default malloc zone during the default malloc zone replacement.
However, Chromium resolves this problem in a different way.

Chromium browser for iOS/macOS consists of a thin driver executable and a dynamic library which contains the main code including PartitionAlloc.
The malloc zone registration is done in two phases.

- [Phase 1](https://source.chromium.org/chromium/chromium/src/+/main:base/allocator/early_zone_registration_apple.cc;drc=d9b8153d34deb93f08d6a61eb12b6cd04486dbd9;l=57):
The executable [registers a placeholder malloc zone and promotes it to the new default malloc zone](https://source.chromium.org/chromium/chromium/src/+/main:base/allocator/early_zone_registration_apple.cc;drc=d9b8153d34deb93f08d6a61eb12b6cd04486dbd9;l=69-101) by [unregistering the existing default malloc zone](https://source.chromium.org/chromium/chromium/src/+/main:base/allocator/early_zone_registration_apple.cc;drc=d9b8153d34deb93f08d6a61eb12b6cd04486dbd9;l=225-233).
The placeholder malloc zone is just a copy of the existing default malloc zone (except for the registration name, locking at fork, etc.), so it can handle allocations made by the existing default malloc zone.
Plus, the replacement is done prior to entering multi-threading.
So there is no race issue.
After that, the existing default malloc zone is re-registered.
- [Phase 2](https://source.chromium.org/chromium/chromium/src/+/main:base/allocator/partition_allocator/src/partition_alloc/shim/allocator_shim_override_apple_default_zone.h;drc=d9b8153d34deb93f08d6a61eb12b6cd04486dbd9;l=359):
When the main dynamic library is loaded, [PartitionAlloc registers a new malloc zone backed by PartitionAlloc, and promotes it to the new default malloc zone](https://source.chromium.org/chromium/chromium/src/+/main:base/allocator/partition_allocator/src/partition_alloc/shim/allocator_shim_override_apple_default_zone.h;drc=d9b8153d34deb93f08d6a61eb12b6cd04486dbd9;l=346-357) by unregistering the placeholder malloc zone.
The existing default malloc zone has already been re-registered, so the promotion process is safe.
By the way, multiple threads have already started running before the main dynamic library is loaded, as its linking dependencies create threads in their static initializers.

Phase 2 is intended to [happen only once](https://source.chromium.org/chromium/chromium/src/+/main:base/allocator/partition_allocator/src/partition_alloc/shim/allocator_shim_override_apple_default_zone.h;drc=d9b8153d34deb93f08d6a61eb12b6cd04486dbd9;l=377-385) along with Phase 1, [except for unit tests](https://source.chromium.org/chromium/chromium/src/+/main:base/allocator/partition_allocator/src/partition_alloc/shim/allocator_shim_override_apple_default_zone.h;drc=d9b8153d34deb93f08d6a61eb12b6cd04486dbd9;l=387-404).
