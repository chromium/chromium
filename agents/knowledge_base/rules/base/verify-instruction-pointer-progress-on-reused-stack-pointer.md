---
name: base/verify-instruction-pointer-progress-on-reused-stack-pointer
rubric: correctness
severity: should_fix
---

**Rule.** When unwinding stack frames on architectures that allow leaf functions
or frames to reuse the stack pointer without advancing it (such as ARM64),
verify that the instruction pointer has advanced if the stack pointer remains
unchanged. Abort unwinding (e.g. return UnwindResult::kAborted) if both the
stack pointer and instruction pointer are identical to the previous frame to
prevent infinite loops on corrupted stacks.

**Why.** A strictly increasing stack pointer normally guarantees forward
progress during stack unwinding. On architectures where leaf functions or system
call dispatchers can legitimately reuse the stack pointer, allowing
non-advancing stack pointers without verifying that the instruction pointer
changes leads to unbounded infinite loops when encountering corrupted or
circular stack memory.

## Examples

Bad:

```cpp
// Allowing SP to equal prev_stack_pointer without checking PC progress allows infinite loops:
if (RegisterContextStackPointer(thread_context) <=
        expected_stack_pointer_range.start ||
    RegisterContextStackPointer(thread_context) >=
        expected_stack_pointer_range.end) {
  return UnwindResult::kAborted;
}
```

Good:

```cpp
if (RegisterContextStackPointer(thread_context) <
        expected_stack_pointer_range.start ||
    RegisterContextStackPointer(thread_context) >=
        expected_stack_pointer_range.end) {
  return UnwindResult::kAborted;
}

if (RegisterContextStackPointer(thread_context) ==
    expected_stack_pointer_range.start) {
#if defined(ARCH_CPU_ARM64)
  if (RegisterContextInstructionPointer(thread_context) ==
      prev_instruction_pointer) {
    // The instruction pointer has not changed, so we are in a loop.
    return UnwindResult::kAborted;
  }
#else
  return UnwindResult::kAborted;
#endif
}
```
