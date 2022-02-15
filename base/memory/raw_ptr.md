# raw_ptr&lt;T&gt; (aka MiraclePtr, aka BackupRefPtr)

`raw_ptr<T>` is a non-owning smart pointer that has improved memory-safety over
over raw pointers.  It behaves just like a raw pointer on platforms where
USE_BACKUP_REF_PTR is off, and almost like one when it's on. The main
difference is that when USE_BACKUP_REF_PTR is enabled, it's zero-initialized and
cleared on destruction and move. (You should continue to explicitly initialize
raw_ptr members to ensure consistent behavior on platforms where USE_BACKUP_REF_PTR
is disabled.) Unlike `std::unique_ptr<T>`, `base::scoped_refptr<T>`, etc., it
doesn’t manage ownership or lifetime of an allocated object - you are still
responsible for freeing the object when no longer used, just as you would
with a raw C++ pointer.

`raw_ptr<T>` is beneficial for security, because it can prevent a significant
percentage of Use-after-Free
(UaF) bugs from being exploitable (by poisoning the freed memory and
quarantining it as long as a dangling `raw_ptr<T>` exists).
`raw_ptr<T>` has limited impact on stability - dereferencing
a dangling pointer remains Undefined Behavior (although poisoning may
lead to earlier, easier to debug crashes).
Note that the security protection is not yet enabled by default.

`raw_ptr<T>` is a part of
[the MiraclePtr project](https://docs.google.com/document/d/1pnnOAIz_DMWDI4oIOFoMAqLnf_MZ2GsrJNb_dbQ3ZBg/edit?usp=sharing)
and currently implements
[the BackupRefPtr algorithm](https://docs.google.com/document/d/1m0c63vXXLyGtIGBi9v6YFANum7-IRC3-dmiYBCWqkMk/edit?usp=sharing).
If needed, please reach out to
[memory-safety-dev@chromium.org](https://groups.google.com/u/1/a/chromium.org/g/memory-safety-dev)
or (Google-internal)
[chrome-memory-safety@google.com](https://groups.google.com/a/google.com/g/chrome-memory-safety)
with questions or concerns.

[TOC]

## When to use |raw_ptr&lt;T&gt;|

[The Chromium C++ Style Guide](../../styleguide/c++/c++.md#non_owning-pointers-in-class-fields)
asks to use `raw_ptr<T>` for class and struct fields in place of
a raw C++ pointer `T*` whenever possible, except in Renderer-only code.
This guide offers more details.

The usage guidelines are *not* enforced currently (the MiraclePtr team will turn
on enforcement via Chromium Clang Plugin after confirming performance results
via Stable channel experiments).  Afterwards we plan to allow
exclusions via:
- [manual-paths-to-ignore.txt](../../tools/clang/rewrite_raw_ptr_fields/manual-paths-to-ignore.txt)
  to exclude at a directory level.  Examples:
    - Renderer-only code (i.e. code in paths that contain `/renderer/` or
      `third_party/blink/public/web/`)
    - Code that cannot depend on `//base`
    - Code in `//ppapi`
- `RAW_PTR_EXCLUSION` C++ attribute to exclude individual fields.  Examples:
    - Cases where `raw_ptr<T>` won't compile (e.g. cases coverd in
      [the "Unsupported cases leading to compile errors" section](#Unsupported-cases-leading-to-compile-errors)).
      Make sure to also look at
      [the "Recoverable compile-time problems" section](#Recoverable-compile_time-problems).
    - Cases where the pointer always points outside of PartitionAlloc
      (e.g.  literals, stack allocated memory, shared memory, mmap'ed memory,
      V8/Oilpan/Java heaps, TLS, etc.).
    - (Very rare) cases that cause regression on perf bots.
    - (Very rare) cases where `raw_ptr<T>` can lead to runtime errors.
      Make sure to look at
      [the "Extra pointer rules" section](#Extra-pointer-rules)
      before resorting to this exclusion.
- No explicit exclusions will be needed for:
    - `const char*`, `const wchar_t*`, etc.
    - Function pointers
    - ObjC pointers

## Examples of using |raw_ptr&lt;T&gt;| instead of raw C++ pointers

Consider an example struct that uses raw C++ pointer fields:

```cpp
struct Example {
  int* int_ptr;
  void* void_ptr;
  SomeClass* object_ptr;
  const SomeClass* ptr_to_const;
  SomeClass* const const_ptr;
};
```

When using `raw_ptr<T>` the struct above would look as follows:

```cpp
#include "base/memory/raw_ptr.h"

struct Example {
  raw_ptr<int> int_ptr;
  raw_ptr<void> void_ptr;
  raw_ptr<SomeClass> object_ptr;
  raw_ptr<const SomeClass> ptr_to_const;
  const raw_ptr<SomeClass> const_ptr;
};
```

In most cases, only the type in the field declaration needs to change.
In particular, `raw_ptr<T>` implements
`operator->`, `operator*` and other operators
that one expects from a raw pointer.
Cases where other code needs to be modified are described in
[the "Recoverable compile-time problems" section](#Recoverable-compile_time-problems)
below.

## Performance

### Performance impact of using |raw_ptr&lt;T&gt;| instead of |T*|

Compared to a raw C++ pointer, on platforms where USE_BACKUP_REF_PTR is on,
`raw_ptr<T>` incurs additional runtime
overhead for initialization, destruction, and assignment (including
`ptr++` and `ptr += ...`).
There is no overhead when dereferencing or extracting a pointer (including
`*ptr`, `ptr->foobar`, `ptr.get()`, or implicit conversions to a raw C++
pointer).
Finally, `raw_ptr<T>` has exactly the same memory footprint as `T*`
(i.e. `sizeof(raw_ptr<T>) == sizeof(T*)`).

One source of the performance overhead is
a check whether a pointer `T*` points to a protected memory pool.
This happens in `raw_ptr<T>`'s
constructor, destructor, and assignment operators.
If the pointed memory is unprotected,
then `raw_ptr<T>` behaves just like a `T*`
and the runtime overhead is limited to the extra check.
(The security protection incurs additional overhead
described in
[the "Performance impact of enabling Use-after-Free protection" section](#Performance-impact-of-enabling-Use_after_Free-protection)
below.)

Some additional overhead comes from setting `raw_ptr<T>` to `nullptr`
when default-constructed, destructed, or moved.

During
[the "Big Rewrite"](https://groups.google.com/a/chromium.org/g/chromium-dev/c/vAEeVifyf78/m/SkBUc6PhBAAJ)
most Chromium `T*` fields have been rewritten to `raw_ptr<T>`
(excluding fields in Renderer-only code).
The cumulative performance impact of such rewrite
has been measured by earlier A/B binary experiments.
There was no measurable impact, except that 32-bit platforms
have seen a slight increase in jankiness metrics
(for more detailed results see
[the document here](https://docs.google.com/document/d/1MfDT-JQh_UIpSQw3KQttjbQ_drA7zw1gQDwU3cbB6_c/edit?usp=sharing)).

### Performance impact of enabling Use-after-Free protection

When the Use-after-Free protection is enabled, then `raw_ptr<T>` has some
additional performance overhead.  This protection is currently disabled
by default.  We will enable the protection incrementally, starting with
more non-Renderer processes first.

The protection can increase memory usage:
- For each memory allocation Chromium's allocator (PartitionAlloc)
  allocates extra 16 bytes (4 bytes to store the BackupRefPtr's
  ref-count associated with the allocation, the rest to maintain
  alignment requirements).
- Freed memory is quarantined and not available for reuse as long
  as dangling `raw_ptr<T>` pointers exist.
- Enabling protection requires additional partitions in PartitionAlloc,
  which increases memory fragmentation.

The protection can increase runtime costs - `raw_ptr<T>`'s constructor,
destructor, and assignment operators (including `ptr++` and `ptr += ...`) need
to maintain BackupRefPtr's ref-count.

## When it is okay to continue using raw C++ pointers

### Unsupported cases leading to compile errors

Using raw_ptr<T> in the following scenarios will lead to build errors.
Continue to use raw C++ pointers in those cases:
- Function pointers
- Pointers to Objective-C objects
- Pointer fields in classes/structs that are used as global or static variables
  (see more details in the
  [Rewrite exclusion statistics](https://docs.google.com/document/d/1uAsWnwy8HfIJhDPSh1efohnqfGsv2LJmYTRBj0JzZh8/edit#heading=h.dg4eebu87wg9)
  )
- Pointer fields that require non-null, constexpr initialization
  (see more details in the
  [Rewrite exclusion statistics](https://docs.google.com/document/d/1uAsWnwy8HfIJhDPSh1efohnqfGsv2LJmYTRBj0JzZh8/edit#heading=h.dg4eebu87wg9)
  )
- Pointer fields in classes/structs that have to be trivially constructible or
  destructible
- Code that doesn’t depend on `//base` (including non-Chromium repositories and
  third party libraries)
- Code in `//ppapi`

### Pointers to unprotected memory (performance optimization)

Using `raw_ptr<T>` offers no security benefits (no UaF protection) for pointers
that don’t point to protected memory (only PartitionAlloc-managed heap allocations
in non-Renderer processes are protected).
Therefore in the following cases raw C++ pointers may be used instead of
`raw_ptr<T>`:
- Pointer fields that can only point outside PartitionAlloc, including literals,
  stack allocated memory, shared memory, mmap'ed memory, V8/Oilpan/Java heaps,
  TLS, etc.
- `const char*` (and `const wchar_t*`) pointer fields, unless you’re convinced
  they can point to a heap-allocated object, not just a string literal
- Pointer fields that can only point to aligned allocations (requested via
  PartitionAlloc’s `AlignedAlloc` or `memalign` family of functions, with
  alignment higher than `base::kAlignment`)
- Pointer fields in Renderer-only code.  (This might change in the future
  as we explore expanding `raw_ptr<T>` usage in https://crbug.com/1273204.)

### Other perf optimizations

As a performance optimization, raw C++ pointers may be used instead of
`raw_ptr<T>` if it would have a significant
[performance impact](#Performance).

### Pointers in locations other than fields

Use raw C++ pointers instead of `raw_ptr<T>` in the following scenarios:
- Pointers in local variables and function/method parameters.
  This includes pointer fields in classes/structs that are used only on the stack.
  (We plan to enforce this in the Chromium Clang Plugin.  Using `raw_ptr<T>`
  here would cumulatively lead to performance regression and the security
  benefit of UaF protection is lower for such short-lived pointers.)
- Pointer fields in unions. (Naive usage this will lead to
  [a C++ compile
  error](https://docs.google.com/document/d/1uAsWnwy8HfIJhDPSh1efohnqfGsv2LJmYTRBj0JzZh8/edit#heading=h.fvvnv6htvlg3).
  Avoiding the error requires the `raw_ptr<T>` destructor to be explicitly
  called before destroying the union, if the field is holding a value. Doing
  this manual destruction wrong might lead to leaks or double-dereferences.)

You don’t have to, but may use `raw_ptr<T>`, in the following scenarios:
- Pointers that are used as an element type of collections/wrappers. E.g.
  `std::vector<T*>` and `std::vector<raw_ptr<T>>` are both okay, but prefer the
  latter if the collection is a class field (note that some of the perf
  optimizations above might still apply and argue for using a raw C++ pointer).


## Extra pointer rules

`raw_ptr<T>` requires following some extra rules compared to a raw C++ pointer:
- Don’t assign invalid, non-null addresses (this includes previously valid and
  now freed memory,
  [Win32 handles](https://crbug.com/1262017), and more). You can only assign an
  address of memory that is allocated at the time of assignment. Exceptions:
    - a pointer to the end of a valid allocation (but not even 1 byte further)
    - a pointer to the last page of the address space, e.g. for sentinels like
      `reinterpret_cast<void*>(-1)`
- Don’t initialize or assign `raw_ptr<T>` memory directly
  (e.g. `reinterpret_cast<ClassWithRawPtr*>(buffer)` or
  `memcpy(reinterpret_cast<void*>(&obj_with_raw_ptr), buffer)`.
- Don’t assign to a `raw_ptr<T>` concurrently, even if the same value.
- Don’t rely on moved-from pointers to keep their old value. Unlike raw
  pointers, `raw_ptr<T>` is cleared upon moving.
- Don't use the pointer after it is destructed. Unlike raw pointers,
  `raw_ptr<T>` is cleared upon destruction. This may happen e.g. when fields are
  ordered such that the pointer field is destructed before the class field whose
  destructor uses that pointer field (e.g. see
  [Esoteric Issues](https://docs.google.com/document/d/14Ol_adOdNpy4Ge-XReI7CXNKMzs_LL5vucDQIERDQyg/edit#heading=h.yoba1l8bnfmv)).
- Don’t assign to a `raw_ptr<T>` until its constructor has run. This may happen
  when a base class’s constructor uses a not-yet-initialized field of a derived
  class (e.g. see
  [Applying MiraclePtr](https://docs.google.com/document/d/1cnpd5Rwesq7DCZiD8FIJfPGHvQN3-Gul6xib_4hwfBg/edit?ts=5ed2d317#heading=h.4ry5d9a6fuxs)).

Some of these would result in undefined behavior (UB) even in the world without
`raw_ptr<T>` (e.g. see
[Field destruction order](https://groups.google.com/a/chromium.org/g/memory-safety-dev/c/3sEmSnFc61I/m/ZtaeWGslAQAJ)),
but you’d likely get away without any consequences. In the `raw_ptr<T>` world,
an obscure crash may occur. Those crashes often manifest themselves as SEGV or
`CHECK` inside `BackupRefPtrImpl::AcquireInternal()` or
`BackupRefPtrImpl::ReleaseInternal()`, but you may also experience memory
corruption or a silent drop of UaF protection.

## Recoverable compile-time problems

### Explicit |raw_ptr.get()| might be needed

If a raw pointer is needed, but an implicit cast from `raw_ptr<SomeClass>` to
`SomeClass*` doesn't work, then the raw pointer needs to be obtained by explicitly
calling `.get()`. Examples:
- `auto* raw_ptr_var = wrapped_ptr_.get()` (`auto*` requires the initializer to
  be a raw pointer)
    - Alternatively you can change `auto*` to `auto&`. Avoid using `auto` as it’ll
      copy the pointer, which incurs a performance overhead.
- `return condition ? raw_ptr : wrapped_ptr_.get();` (ternary operator needs
  identical types in both branches)
- `base::WrapUniquePtr(wrapped_ptr_.get());` (implicit cast doesn't kick in for
  arguments in templates)
- `printf("%p", wrapped_ptr_.get());` (can't pass class type arguments to
  variadic functions)
- `reinterpret_cast<SomeClass*>(wrapped_ptr_.get())` (`const_cast` and
  `reinterpret_cast` sometimes require their argument to be a raw pointer;
  `static_cast` should "Just Work")
- `T2 t2 = t1_wrapped_ptr_.get();` (where there is an implicit conversion
  constructor `T2(T1*)` the compiler can handle one implicit conversion, but not
  two)
- In general, when type is inferred by a compiler and then used in a context
  where a pointer is expected.

### Out-of-line constructor/destructor might be needed

Out-of-line constructor/destructor may be newly required by the chromium style
clang plugin.  Error examples:
- `error: [chromium-style] Complex class/struct needs an explicit out-of-line
  destructor.`
- `error: [chromium-style] Complex class/struct needs an explicit out-of-line
  constructor.`

`raw_ptr<T>` uses a non-trivial constructor/destructor, so classes that used to
be POD or have a trivial destructor may require an out-of-line
constructor/destructor to satisfy the chromium style clang plugin.


### In-out arguments need to be refactored

Due to implementation difficulties,
`raw_ptr<T>` doesn't support an address-of operator.
This means that the following code will not compile:

```cpp
void GetSomeClassPtr(SomeClass** out_arg) {
  *out_arg = ...;
}

struct MyStruct {
  void Example() {
    GetSomeClassPtr(&wrapped_ptr_);  // <- won't compile
  }

  raw_ptr<SomeClass> wrapped_ptr_;
};
```

The typical fix is to change the type of the out argument:

```cpp
void GetSomeClassPtr(raw_ptr<SomeClass>* out_arg) {
  *out_arg = ...;
}
```

Similarly this code:

```cpp
void FillPtr(SomeClass*& out_arg) {
  out_arg = ...;
}
```

would have to be changed to this:

```cpp
void FillPtr(raw_ptr<SomeClass>& out_arg) {
  out_arg = ...;
}
```

Similarly this code:

```cpp
SomeClass*& GetPtr() {
  return wrapper_ptr_;
}
```

would have to be changed to this:

```cpp
raw_ptr<SomeClass>& GetPtr() {
  return wrapper_ptr_;
}
```

### Modern |nullptr| is required

As recommended by the Google C++ Style Guide,
[use nullptr instead of NULL](https://google.github.io/styleguide/cppguide.html#0_and_nullptr/NULL) -
the latter might result in compile-time errors when used with `raw_ptr<T>`.

Example:

```cpp
struct SomeStruct {
  raw_ptr<int> ptr_field;
};

void bar() {
  SomeStruct some_struct;
  some_struct.ptr_field = NULL;
}
```

Error:
```err
../../base/memory/checked_ptr_unittest.cc:139:25: error: use of overloaded
operator '=' is ambiguous (with operand types raw_ptr<int>' and 'long')
  some_struct.ptr_field = NULL;
  ~~~~~~~~~~~~~~~~~~~~~ ^ ~~~~
../../base/memory/raw_ptr.h:369:29: note: candidate function
  ALWAYS_INLINE raw_ptr& operator=(std::nullptr_t) noexcept {
                         ^
../../base/memory/raw_ptr.h:374:29: note: candidate function
  ALWAYS_INLINE raw_ptr& operator=(T* p)
                         noexcept {
```

### [rare] Explicit overload or template specialization for |raw_ptr&lt;T&gt;|

In rare cases, the default template code won’t compile when `raw_ptr<...>` is
substituted for a template argument.  In such cases, it might be necessary to
provide an explicit overload or template specialization for `raw_ptr<T>`.

Example (more details in
[Applying MiraclePtr](https://docs.google.com/document/d/1cnpd5Rwesq7DCZiD8FIJfPGHvQN3-Gul6xib_4hwfBg/edit?ts=5ed2d317#heading=h.o2pf3fg0zzf) and the
[Add CheckedPtr support for cbor_extract::Element](https://chromium-review.googlesource.com/c/chromium/src/+/2224954)
CL):

```cpp
// An explicit overload (taking raw_ptr<T> as an argument)
// was needed below:
template <typename S>
constexpr StepOrByte<S> Element(
    const Is required,
    raw_ptr<const std::string> S::*member,  // <- HERE
    uintptr_t offset) {
  return ElementImpl<S>(required, offset, internal::Type::kString);
}
```
