---
name: base/use-ptrauth-strip-for-thread-context-registers
rubric: correctness
severity: should_fix
---

**Rule.** When extracting register values (such as stack pointer, frame pointer,
or program counter) from thread context structures on ARM64, do not attempt to
authenticate them with ptrauth_auth_data(...) using an assumed discriminator
(such as 0). Use ptrauth_strip(...) to obtain the raw stripped pointer address
instead.

**Why.** Registers in thread context states are often signed by the kernel or
ABI using execution context discriminators inaccessible to profiling or
unwinding code. Calling ptrauth_auth_data(...) with an assumed discriminator
fails authentication and corrupts the pointer or triggers a trap, whereas
profiler and unwinding tools only require the raw stripped address.

## Examples

Bad:

```cpp
uintptr_t sp = context->__opaque_sp
    ? reinterpret_cast<uintptr_t>(ptrauth_auth_data(
          context->__opaque_sp, ptrauth_key_asda, 0))
    : 0;
```

Good:

```cpp
void* val = ptrauth_strip(context->__opaque_sp, ptrauth_key_asda);
uintptr_t sp = AsUintPtr(&val);
```
