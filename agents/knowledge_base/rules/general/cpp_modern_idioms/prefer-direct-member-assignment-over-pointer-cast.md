---
name: general/prefer-direct-member-assignment-over-pointer-cast
rubric: readability
severity: should_fix
---

**Rule.** When accessing or assigning values to struct or class members (such as
in value-based getters and setters), use direct member access or assignment
rather than taking the member's address and casting it through reinterpret_cast
or pointer-casting helpers. Rely on standard member access, assignment, and
implicit conversions whenever direct access suffices.

**Why.** Pointer-casting wrappers and lvalue reference casts are unnecessary for
value-based getters and setters. Using direct member access and standard
assignment avoids redundant reinterpret_cast operations, eliminates type-punning
strict-aliasing hazards, and simplifies the code.

## Examples

Bad:

```cpp
inline void SetRegisterContextStackPointer(mcontext_t* context, uintptr_t val) {
  AsUintPtr(&context->arm_sp) = val;
}

inline uintptr_t RegisterContextStackPointer(const mcontext_t* context) {
  return *reinterpret_cast<const uintptr_t*>(&context->arm_sp);
}
```

Good:

```cpp
inline void SetRegisterContextStackPointer(mcontext_t* context, uintptr_t val) {
  context->arm_sp = val;
}

inline uintptr_t RegisterContextStackPointer(const mcontext_t* context) {
  return context->arm_sp;
}
```
