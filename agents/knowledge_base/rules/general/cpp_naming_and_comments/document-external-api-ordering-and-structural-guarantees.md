---
name: general/document-external-api-ordering-and-structural-guarantees
rubric: readability
severity: nit
---

**Rule.** When code relies on subtle ordering, sequencing, or structural layout
guarantees from external libraries or OS APIs, adapts low-level platform
helpers, exposes platform-derived metrics, or implements ABI/compiler
requirements, document the contract in code comments. Explicitly identify
underlying OS API names, source struct members, stability or signal-safety
caveats, and intentional deviations from canonical implementations. When citing
external specifications, ABI standards, or compiler documentation, link directly
to the specific authoritative documentation section or URL rather than providing
vague references or naming headers that do not define the behavior.

**Why.** External OS platform APIs, ABI conventions, and compiler mechanisms
often have subtle contracts, unstable kernel structures, or specific safety
constraints. Retaining direct URLs to authoritative documentation sections,
naming underlying APIs/fields, and documenting intentional deviations prevents
incorrect refactorings, misinterpretations, and compatibility regressions across
platforms.

## Examples

Bad:

```cpp
// 1. Relying on OS API traversal guarantees without documentation:
while (::HeapWalk(crt_heap, &heap_entry) != FALSE) {
  const uintptr_t entry_addr = reinterpret_cast<uintptr_t>(heap_entry.lpData);
  if ((heap_entry.wFlags & PROCESS_HEAP_ENTRY_BUSY) != 0) {
    if (entry_addr < last_region_start || entry_addr >= last_region_end) {
      crt_heap_info->committed_size += heap_entry.cbData + heap_entry.cbOverhead;
    }
  }
}

// 2. Exposing OS metrics without naming source APIs/fields or linking docs:
#if BUILDFLAG(IS_WIN)
base::ByteSize available_commit_bytes;  // Commit space limit.
#endif

// 3. Adapting platform APIs without documenting vendor references or caveats:
bool BeingDebugged() {
  int mib[] = {CTL_KERN, KERN_PROC, KERN_PROC_PID, getpid()};
  struct kinfo_proc info;
  size_t info_size = sizeof(info);
  return sysctl(mib, 4, &info, &info_size, NULL, 0) == 0 && (info.kp_proc.p_flag & P_TRACED);
}

// 4. Vague ABI citation or naming headers that do not define behavior:
// Per the Objective-C ABI for arm64e (see LLVM's Pointer Authentication
// documentation and `ptrauth.h`), block function pointers are signed...
```

Good:

```cpp
// 1. Documenting OS traversal guarantees with vendor doc link:
// HeapWalk emits a PROCESS_HEAP_REGION header before the blocks inside
// that region; large VirtualAlloc-backed allocations appear as orphan busy entries.
// See: https://learn.microsoft.com/en-us/windows/win32/api/minwinbase/ns-minwinbase-process_heap_entry
while (::HeapWalk(crt_heap, &heap_entry) != FALSE) {
  const uintptr_t entry_addr = reinterpret_cast<uintptr_t>(heap_entry.lpData);
  if ((heap_entry.wFlags & PROCESS_HEAP_ENTRY_BUSY) != 0) {
    if (entry_addr < last_region_start || entry_addr >= last_region_end) {
      info->committed_size += heap_entry.cbData + heap_entry.cbOverhead;
    }
  }
}

// 2. Documenting OS metric sources, field names, and vendor doc link:
#if BUILDFLAG(IS_WIN)
// Retrieved via MEMORYSTATUSEX (ullAvailPageFile). See:
// https://learn.microsoft.com/en-us/windows/win32/api/sysinfoapi/ns-sysinfoapi-memorystatusex
base::ByteSize available_commit_bytes;
#endif

// 3. Documenting vendor link, unstable struct caveats, and base deviations:
// Based on Apple's recommended method: http://developer.apple.com/qa/qa2004/qa1361.html
// Note: Not signal-safe unlike base::debug::BeingDebugged() as this is test-only.
bool BeingDebugged() {
  int mib[] = {CTL_KERN, KERN_PROC, KERN_PROC_PID, getpid()};
  // Caution: struct kinfo_proc is marked __APPLE_API_UNSTABLE.
  struct kinfo_proc info;
  size_t info_size = sizeof(info);
  int result = sysctl(mib, std::size(mib), &info, &info_size, NULL, 0);
  return result == 0 && (info.kp_proc.p_flag & P_TRACED) != 0;
}

// 4. Linking directly to authoritative compiler/ABI specification section:
// Per the Objective-C ABI for arm64e, block function pointers are signed
// using their storage address (x0 + 16) as the diversifier and discriminator 0.
// See https://clang.llvm.org/docs/PointerAuthentication.html#blocks
```
