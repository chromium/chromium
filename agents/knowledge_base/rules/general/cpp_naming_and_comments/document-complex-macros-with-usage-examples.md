---
name: general/document-complex-macros-with-usage-examples
rubric: readability
severity: nit
---

**Rule.** When defining non-trivial utility, boilerplate-generating, compiler
attribute, sanitizer wrapper, or language-interop macros in C++ header files
(such as compiler_specific.h or PartitionAlloc counterparts) or Rust modules,
document each macro with doc comments (// or ///). Explain the macro's behavior,
include official documentation reference links (See also:) for
toolchain/sanitizer features, and provide copy-pasteable usage examples (Usage:
or Example:) showing declaration, definition, or call-site invocation syntax.

**Why.** Complex boilerplate macros, cross-language FFI bindings, compiler
attributes, and sanitizer wrappers can be opaque, toolchain-dependent, and
difficult to invoke correctly from macro definitions alone. Standardized doc
comments with official documentation links and concrete invocation examples
clarify expected usage patterns, prevent misuse, and make toolchain-specific
abstractions auditable across the codebase.

## Examples

Bad:

```cpp
// Bad: Undocumented compiler/sanitizer macro in compiler_specific.h
#if defined(MEMORY_SANITIZER)
#define PA_MSAN_CHECK_MEM_IS_INITIALIZED(p, size) \
  __msan_check_mem_is_initialized(p, size)
#else
#define PA_MSAN_CHECK_MEM_IS_INITIALIZED(p, size)
#endif

// Bad: Boilerplate wrapper macro without usage examples
#define SWIFT_DECLARE_INTEROP_WRAPPER(NAME, WRAPPED_CLASS) \
  class NAME : public WRAPPED_CLASS {                      \
   public:                                                 \
    using WRAPPED_CLASS::WRAPPED_CLASS;                    \
  };
```

Good:

```cpp
// Checks if the pointer `p` and the `size` bytes after it are initialized.
//
// See also:
//   https://github.com/google/sanitizers/wiki/MemorySanitizer
//
// Usage:
//   PA_MSAN_CHECK_MEM_IS_INITIALIZED(&ptr, sizeof(ptr));
#if defined(MEMORY_SANITIZER)
#define PA_MSAN_CHECK_MEM_IS_INITIALIZED(p, size) \
  __msan_check_mem_is_initialized(p, size)
#else
#define PA_MSAN_CHECK_MEM_IS_INITIALIZED(p, size)
#endif

// Creates a wrapper class to work around swift compiler interop limitations.
//
// Example:
//   In .h:  SWIFT_DECLARE_INTEROP_WRAPPER(IntCallback, base::RepeatingCallback<void(int)>);
//   In .cc: SWIFT_DEFINE_INTEROP_WRAPPER(IntCallback, base::RepeatingCallback<void(int)>);
#define SWIFT_DECLARE_INTEROP_WRAPPER(NAME, WRAPPED_CLASS) \
  class NAME : public WRAPPED_CLASS {                      \
   public:                                                 \
    using WRAPPED_CLASS::WRAPPED_CLASS;                    \
  };
```
