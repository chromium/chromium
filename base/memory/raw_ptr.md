# raw_ptr&lt;T&gt; (aka. MiraclePtr, aka. BackupRefPtr, aka. BRP)

## Quick rules

Before telling you what `raw_ptr<T>` is, we'd like you to follow one simple
rule: think of it as a raw C++ pointer. In particular:
- Initialize it yourself, don't assume the constructor default-initializes it
  (it may or may not). (Always use the `raw_ptr<T> member_ = nullptr;` form of
  initialization rather than the so-called uniform initialization form
  (empty braces) `raw_ptr<T> member_{};` whose meaning varies with the
  implementation.)
- Don't assume that moving clears the pointer (it may or may not).
- The owner of the memory must free it when the time is right, don't assume
  `raw_ptr<T>` will free it for  you (it won't). Unlike `std::unique_ptr<T>`,
  `base::scoped_refptr<T>`, etc., it does not manage ownership or lifetime of
  an allocated object.
  - if the pointer is the owner of the memory, consider using an alternative
    smart pointer.
- Don't assume `raw_ptr<T>` will protect you from freeing memory too early (it
  likely will, but there are gotchas; one of them is that dereferencing will
  result in other type of undefined behavior).

(There are other, much more subtle rules that you should follow, but they're
harder to accidentally violate, hence discussed in the further section
["Extra pointer rules"](#Extra-pointer-rules).)

## What is |raw_ptr&lt;T&gt;|

`raw_ptr<T>` is a part of
[the MiraclePtr project](https://docs.google.com/document/d/1pnnOAIz_DMWDI4oIOFoMAqLnf_MZ2GsrJNb_dbQ3ZBg/edit?usp=sharing)
and currently implements
[the BackupRefPtr algorithm](https://docs.google.com/document/d/1m0c63vXXLyGtIGBi9v6YFANum7-IRC3-dmiYBCWqkMk/edit?usp=sharing).
If needed, please reach out to
[memory-safety-dev@chromium.org](https://groups.google.com/a/chromium.org/g/memory-safety-dev)
or (Google-internal)
[chrome-memory-safety@google.com](https://groups.google.com/a/google.com/g/chrome-memory-safety)
with questions or concerns.

`raw_ptr<T>` is a non-owning smart pointer that has improved memory-safety over
raw pointers.  It behaves just like a raw pointer on platforms where
USE_RAW_PTR_BACKUP_REF_IMPL is off, and almost like one when it's on. The main
difference is that when USE_RAW_PTR_BACKUP_REF_IMPL is enabled, `raw_ptr<T>`
is beneficial for security, because it can prevent a significant percentage of
Use-after-Free (UaF) bugs from being exploitable. It achieves this by
quarantining the freed memory as long as any dangling `raw_ptr<T>` pointing to
it exists, and poisoning it (with
[0xEF..EF](https://source.chromium.org/chromium/chromium/src/+/main:base/allocator/partition_allocator/src/partition_alloc/partition_alloc_constants.h;l=488;drc=b5a738b11528b81c4cc2d522bfac88716c8aac49)
pattern).

Note that the sheer act of dereferencing a dangling pointer won't
crash, but poisoning increases chances that a subsequent usage of read memory
will crash (particularly if the read poison is interpreted as a pointer and
dereferenced thereafter), thus giving us a chance to investigate and fix.
Having said that, we want to emphasize that dereferencing a dangling pointer
remains an Undefined Behavior.

`raw_ptr<T>` protection is enabled by default in all non-Renderer processes, on:
- Android (incl. AndroidWebView, Android WebEngine, & Android ChromeCast)
- Windows
- ChromeOS (incl. Ash & Lacros)
- macOS
- Linux
- Fuchsia

In particular, it isn't yet enabled by default on:
- iOS
- Linux CastOS (Nest hardware)

For the source of truth, both `enable_backup_ref_ptr_support` and `enable_backup_ref_ptr_feature_flag` need to enabled.
Please refer to the following files: [build_overrides/partition_alloc.gni](https://source.chromium.org/chromium/chromium/src/+/main:build_overrides/partition_alloc.gni) and [partition_alloc.gni](https://source.chromium.org/chromium/chromium/src/+/main:base/allocator/partition_allocator/partition_alloc.gni;l=5?q=partition_alloc.gni&sq=&ss=chromium)


[TOC]

## When to use |raw_ptr&lt;T&gt;|

[The Chromium C++ Style Guide](../../styleguide/c++/c++.md#non_owning-pointers-in-class-fields)
asks to use `raw_ptr<T>` for class and struct fields in place of
a raw C++ pointer `T*` whenever possible, except in Renderer-only code.
This guide offers more details.

The usage guidelines are currently enforced via Chromium Clang Plugin. We allow
exclusions via:
- `RAW_PTR_EXCLUSION` C++ attribute to exclude individual fields.  Examples:
    - Cases where `raw_ptr<T>` won't compile (e.g. cases covered in
      [the "Unsupported cases leading to compile errors" section](#Unsupported-cases-leading-to-compile-errors)).
      Make sure to also look at
      [the "Recoverable compile-time problems" section](#Recoverable-compile-time-problems).
    - Cases where the pointer always points outside of PartitionAlloc
      (e.g.  literals, stack allocated memory, shared memory, mmap'ed memory,
      V8/Oilpan/Java heaps, TLS, etc.).
    - (Very rare) cases that cause perf regression.
    - (Very rare) cases where `raw_ptr<T>` can lead to runtime errors.
      Make sure to look at
      [the "Extra pointer rules" section](#Extra-pointer-rules)
      before resorting to this exclusion.
- [RawPtrManualPathsToIgnore.h](../../tools/clang/plugins/RawPtrManualPathsToIgnore.h)
  to exclude at a directory level (NOTE, use it as last resort, and be aware
  it'll require a Clang plugin roll).  Examples:
    - Renderer-only code (i.e. code in paths that contain `/renderer/` or
      `third_party/blink/public/web/`)
    - Code that cannot depend on `//base`
    - Code in `//ppapi`
- No explicit exclusions are needed for:
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
[the "Recoverable compile-time problems" section](#Recoverable-compile-time-problems)
below.

## Performance

### Performance impact of using |raw_ptr&lt;T&gt;| instead of |T\*|

Compared to a raw C++ pointer, on platforms where USE_RAW_PTR_BACKUP_REF_IMPL
is on, `raw_ptr<T>` incurs additional runtime
overhead for initialization, destruction, and assignment (including
`ptr++`, `ptr += ...`, etc.).
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
and the runtime overhead is limited to that extra check.
(The security protection incurs additional overhead
described in
[the "Performance impact of enabling Use-after-Free protection" section](#Performance-impact-of-enabling-Use-after-Free-protection)
below.)

Some additional overhead comes from setting `raw_ptr<T>` to `nullptr`
when default-constructed, destructed, or moved. (Yes, we said above to not rely
on it, but to be precise this will always happen when
USE_RAW_PTR_BACKUP_REF_IMPL is on; no guarantees otherwise.)

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

### Performance impact of enabling Use-after-Free protection {#Performance-impact-of-enabling-Use-after-Free-protection}

When the Use-after-Free protection is enabled, then `raw_ptr<T>` has some
additional performance overhead.

The protection can increase memory usage:
- For each memory allocation Chromium's allocator (PartitionAlloc)
  carves out extra 4 bytes. (That doesn't necessarily mean that each allocation
  grows by 4B. Allocation sizes come from predefined buckets, so it's possible
  for an allocation to stay within the same bucket and incur no additional
  overhead, or hop over to the next bucket and incur much higher overhead.)
- Freed memory is quarantined and not available for reuse as long
  as dangling `raw_ptr<T>` pointers exist. (In practice this overhead has been
  observed to be low, but on a couple occasions it led to significant memory
  leaks, fortunately caught early.)

The protection increases runtime costs - `raw_ptr<T>`'s constructor,
destructor, and assignment operators need to maintain BackupRefPtr's ref-count
(atomic increment/decrement). `ptr++`, `ptr += ...`, etc. don't need to do that,
but instead have to incur the cost
of verifying that resulting pointer stays within the same allocation (important
for BRP integrity).

## When it is okay to continue using raw C++ pointers

### Unsupported cases leading to compile errors {#Unsupported-cases-leading-to-compile-errors}

Continue to use raw C++ pointers in the following cases, which may otherwise
result in compile errors:
- Function pointers
- Pointers to Objective-C objects
- Pointer fields in classes/structs that are used as global, static, or
  `thread_local` variables (see more details in the
  [Rewrite exclusion statistics](https://docs.google.com/document/d/1uAsWnwy8HfIJhDPSh1efohnqfGsv2LJmYTRBj0JzZh8/edit#heading=h.dg4eebu87wg9)
  )
- Pointers in unions, as well as pointer fields in classes/structs that are used
  in unions (side note, absl::variant is strongly preferred)
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
- Pointer fields in certain renderer code. Specifically, we disallow usage in

``` none
third_party/blink/renderer/core/
third_party/blink/renderer/platform/heap/
third_party/blink/renderer/platform/wtf/
```

### Other perf optimizations

As a performance optimization, raw C++ pointers may be used instead of
`raw_ptr<T>` if it would have a significant
[performance impact](#Performance).

### Pointers in locations other than fields

Use raw C++ pointers instead of `raw_ptr<T>` in the following scenarios:
- Pointers in local variables and function parameters and return values. This
  includes pointer fields in classes/structs that are used only on the stack.
  (Using `raw_ptr<T>` here would cumulatively lead to performance regression and
  the security benefit of UaF protection is lower for such short-lived
  pointers.)
- Pointer fields in unions. However, note that a much better, modern alternative
  is `absl::variant` + `raw_ptr<T>`. If use of C++ union is absolutely
  unavoidable, prefer a regular C++ pointer: incorrect management of a
  `raw_ptr<T>` field can easily lead to ref-count corruption.
- Pointers whose addresses are used only as identifiers and which are
  never dereferenced (e.g. keys in a map). There is a performance gain
  by not using `raw_ptr` in this case; prefer to use `uintptr_t` to
  emphasize that the entity can dangle and must not be dereferenced. (NOTE,
  this is a dangerous practice irrespective of raw_ptr usage, as there is a risk
  of memory being freed and another pointer allocated with the same address!)

You don’t have to, but may use `raw_ptr<T>`, in the following scenarios:
- Pointers that are used as an element type of collections/wrappers. E.g.
  `std::vector<T*>` and `std::vector<raw_ptr<T>>` are both okay, but prefer the
  latter if the collection is a class field (note that some of the perf
  optimizations above might still apply and argue for using a raw C++ pointer).

### Signal Handlers

`raw_ptr<T>` assumes that the allocator's data structures are in a consistent
state. Signal handlers can interrupt in the middle of an allocation operation;
therefore, `raw_ptr<T>` should not be used in signal handlers.

## Extra pointer rules {#Extra-pointer-rules}

`raw_ptr<T>` requires following some extra rules compared to a raw C++ pointer:
- Don’t assign invalid, non-null addresses (this includes previously valid and
  now freed memory,
  [Win32 handles](https://crbug.com/1262017), and more). You can only assign an
  address of memory that is valid at the time of assignment. Exceptions:
    - a pointer to the end of a valid allocation (but not even 1 byte further)
    - a pointer to the last page of the address space, e.g. for sentinels like
      `reinterpret_cast<void*>(-1)`
- Don’t initialize or assign `raw_ptr<T>` memory directly
  (e.g. `reinterpret_cast<ClassWithRawPtr*>(buffer)` or
  `memcpy(reinterpret_cast<void*>(&obj_with_raw_ptr), buffer)`.
- Don’t assign to a `raw_ptr<T>` concurrently, even if the same value.
- Don’t rely on moved-from pointers to keep their old value. Unlike raw
  pointers, `raw_ptr<T>` may be cleared upon moving.
- Don't use the pointer after it is destructed. Unlike raw pointers,
  `raw_ptr<T>` may be cleared upon destruction. This may happen e.g. when fields
  are ordered such that the pointer field is destructed before the class field
  whose destructor uses that pointer field (e.g. see
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
`CHECK` inside `RawPtrBackupRefImpl::AcquireInternal()` or
`RawPtrBackupRefImpl::ReleaseInternal()`, but you may also experience memory
corruption or a silent drop of UaF protection.

## Pointer Annotations

### The AllowPtrArithmetic trait

In an ideal world, a raw_ptr would point to a single object, rather than to
a C-style array of objects accessed via pointer arithmetic, since the latter
is best handled via a C++ construct such as base::span<> or std::vector<>.
raw_ptrs upon which such operations are performed and for which conversion is
desirable have been tagged with the AllowPtrArithmetic trait. That all such
pointer are tagged can be enforced by setting the GN build arg
enable_pointer_arithmetic_trait_check=true.

### The AllowUninitialized trait

When building Chromium, raw_ptrs are always nullptr initialized, either as
the result of specific implementation that requires it (e.g. BackupRefPtr),
or as the result of build flags (to enforce consistency). However, we provide
an opt-out to allow third-party code to skip this step (where possible). Use
this trait sparingly.

## Recoverable compile-time problems {#Recoverable-compile-time-problems}

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
- `TemplatedFunction(wrapped_ptr_.get());` (implicit cast doesn't kick in for
  `T*` arguments in templates)
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

The typical fix is to change the type of the out argument
(see also [an example CL here](https://crrev.com/c/4545743)):

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


In case you cannot refactor the in-out arguments (e.g. third party library), you
may use `raw_ptr.AsEphemeralRawAddr()` to obtain *extremely* short-lived
`T**` or `T*&`. You should not treat `T**` obtained via
`raw_ptr.AsEphemeralRawAddr()` as a normal pointer pointer, and must follow
these requirements.

- Do NOT store `T**` or `T*&` anywhere, even as a local variable.
  - It will become invalid very quickly and can cause dangling pointer issue
- Do NOT use `raw_ptr<T>`, `T**` or `T*&` multiple times within an expression.
  - The implementation assumes raw_ptr<T> is never accessed when `T**` or `T*&`
    is alive.

```cpp
void GetSomeClassPtr(SomeClass** out_arg) {
  *out_arg = ...;
}
void FillPtr(SomeClass*& out_arg) {
  out_arg = ...;
}
void Foo() {
  raw_ptr<SomeClass> ptr;
  GetSomeClassPtr(&ptr.AsEphemeralRawAddr());
  FillPtr(ptr.AsEphemeralRawAddr()); // Implicitly converted into |SomeClass*&|.
}
```

Technically, `raw_ptr.AsEphemeralRawAddr()` generates a temporary instance of
`raw_ptr<T>::EphemeralRawAddr`, which holds a temporary copy of `T*`.
`T**` and `T*&` points to a copied version of the original pointer and
any modification made via `T**` or `T*&` is written back on destruction of
`EphemeralRawAddr` instance.
C++ guarantees a temporary object returned by `raw_ptr.AsEphemeralRawAddr()`
lives until completion of evaluation of "full-expression" (i.e. the outermost
expression). This makes it possible to use `T**` and `T*&` within single
expression like in-out param.

```cpp
struct EphemeralRawAddr {
  EphemeralRawAddr(raw_ptr& ptr): copy(ptr.get()), original(ptr) {}
  ~EphemeralRawAddr() {
    original = copy;
    copy = nullptr;
  }

  T** operator&() { return &copy; }
  operator T*&() { return copy; }

  T* copy;
  raw_ptr& original;  // Original pointer.
};
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

## AddressSanitizer support

For years, AddressSanitizer has been the main tool for diagnosing memory
corruption issues in Chromium. MiraclePtr alters the security properties of some
of some such issues, so ideally it should be integrated with ASan. That way an
engineer would be able to check whether a given use-after-free vulnerability is
covered by the protection without having to switch between ASan and non-ASan
builds.

Unfortunately, MiraclePtr relies heavily on PartitionAlloc, and ASan needs its
own allocator to work. As a result, the default implementation of `raw_ptr<T>`
can't be used with ASan builds. Instead, a special version of `raw_ptr<T>` has
been implemented, which is based on the ASan quarantine and acts as a
sufficiently close approximation for diagnostic purposes. At crash time, the
tool will tell the user if the dangling pointer access would have been protected
by MiraclePtr *in a regular build*.

You can configure the diagnostic tool by modifying the parameters of the feature
flag `PartitionAllocBackupRefPtr`. For example, launching Chromium as follows:

```
path/to/chrome --enable-features=PartitionAllocBackupRefPtr:enabled-processes/browser-only/asan-enable-dereference-check/true/asan-enable-extraction-check/true/asan-enable-instantiation-check/true
```

activates all available checks in the browser process.

### Available checks

MiraclePtr provides ASan users with three kinds of security checks, which differ
in when a particular check occurs:

#### Dereference

This is the basic check type that helps diagnose regular heap-use-after-free
bugs. It's enabled by default.

#### Extraction

The user will be warned if a dangling pointer is extracted from a `raw_ptr<T>`
variable. If the pointer is then dereferenced, an ASan error report will follow.
In some cases, extra work on the reproduction case is required to reach the
faulty memory access. However, even without memory corruption, relying on the
value of a dangling pointer may lead to problems. For example, it's a common
(anti-)pattern in Chromium to use a raw pointer as a key in a container.
Consider the following example:

```
std::map<T*, std::unique_ptr<Ext>> g_map;

struct A {
  A() {
    g_map[this] = std::make_unique<Ext>(this);
  }

  ~A() {
    g_map.erase(this);
  }
};

raw_ptr<A> dangling = new A;
// ...
delete dangling.get();
A* replacement = new A;
// ...
auto it = g_map.find(dangling);
if (it == g_map.end())
  return 0;
it->second.DoStuff();
```

Depending on whether the allocator reuses the same memory region for the second
`A` object, the program may inadvertently call `DoStuff()` on the wrong `Ext`
instance. This, in turn, may corrupt the state of the program or bypass security
controls if the two `A` objects belong to different security contexts.

Given the proportion of false positives reported in the mode, it is disabled by
default. It's mainly intended to be used by security researchers who are willing
to spend a significant amount of time investigating these early warnings.

#### Instantiation

This check detects violations of the rule that when instantiating a `raw_ptr<T>`
from a `T*` , it is only allowed if the `T*` is a valid (i.e. not dangling)
pointer. This rule exists to help avoid an issue called "pointer laundering"
which can result in unsafe `raw_ptr<T>` instances that point to memory that is
no longer in quarantine. This is important, since subsequent use of these
`raw_ptr<T>` might appear to be safe.

In order for "pointer laundering" to occur, we need (1) a dangling `T*`
(pointing to memory that has been freed) to be assigned to a `raw_ptr<T>`, while
(2) there is no other `raw_ptr<T>` pointing to the same object/allocation at the
time of assignment.

The check only detects (1), a dangling `T*` being assigned to a `raw_ptr<T>`, so
in order to determine whether "pointer laundering" has occurred, we need to
determine whether (2) could plausibly occur, not just in the specific
reproduction testcase, but in the more general case.

In the absence of thorough reasoning about (2), the assumption here should be
that any failure of this check is a security issue of the same severity as an
unprotected use-after-free.

### Protection status

When ASan generates a heap-use-after-free report, it will include a new section
near the bottom, which starts with the line `MiraclePtr Status: <status>`. At
the moment, it has three possible options:

#### Protected

The system is sufficiently confident that MiraclePtr makes the discovered issue
unexploitable. In the future, the security severity of such bugs will be
reduced.

#### Manual analysis required

Dangling pointer extraction was detected before the crash, but there might be
extra code between the extraction and dereference. Most of the time, the code in
question will look similar to the following:

```
struct A {
  raw_ptr<T> dangling_;
};

void trigger(A* a) {
  // ...
  T* local = a->dangling_;
  DoStuff();
  local->DoOtherStuff();
  // ...
}
```

In this scenario, even though `dangling_` points to freed memory, that memory
is protected and will stay in quarantine until `dangling_` (and all other
`raw_ptr<T>` variables pointing to the same region) changes its value or gets
destroyed. Therefore, the expression `a_->dangling->DoOtherStuff()` wouldn't
trigger an exploitable use-after-free.

You will need to make sure that `DoStuff()` is sufficiently trivial and can't
(not only for the particular reproduction case, but *even in principle*) make
`dangling_` change its value or get destroyed. If that's the case, the
`DoOtherStuff()` call may be considered protected. The tool will provide you
with the stack trace for both the extraction and dereference events.

#### Not protected

The dangling `T*` doesn't appear to originate from a `raw_ptr<T>` variable,
which means MiraclePtr can't prevent this issue from being exploited. In
practice, there may still be a `raw_ptr<T>` in a different part of the code that
protects the same allocation indirectly, but such protection won't be considered
robust enough to impact security-related decisions.

### Limitations

The main limitation of MiraclePtr in ASan builds is the main limitation of ASan
itself: the capacity of the quarantine is limited. Eventually, every allocation
in quarantine will get reused regardless of whether there are still references
to it.

In the context of MiraclePtr combined with ASan, it's a problem when:

1. A heap allocation that isn't supported by MiraclePtr is made. At the moment,
   the only such class is allocations made early during the process startup
   before MiraclePtr can be activated.
2. Its address is assigned to a `raw_ptr<T>` variable.
3. The allocation gets freed.
4. A new allocation is made in the same memory region as the first one, but this
   time it is supported.
5. The second allocation gets freed.
6. The `raw_ptr<T>` variable is accessed.

In this case, MiraclePtr will incorrectly assume the memory access is protected.
Luckily, considering the small number of unprotected allocations in Chromium,
the size of the quarantine, and the fact that most reproduction cases take
relatively short time to run, the odds of this happening are very low.

The problem is relatively easy to spot if you look at the ASan report: the
allocation and deallocation stack traces won't be consistent across runs and
the allocation type won't match the use stack trace.

If you encounter a suspicious ASan report, it may be helpful to re-run Chromium
with an increased quarantine capacity as follows:

```
ASAN_OPTIONS=quarantine_size_mb=1024 path/to/chrome
```

## Appendix: Is raw_ptr Live?

![Diagram showing how both code support and feature flag must be present
  for raw_ptr to be BRP.](./raw_ptr_liveness.png)

Note that

*   [`RawPtrNoOpImpl`][raw-ptr-noop-impl] is thought to have no
    overhead. However, this has yet to be verified.

*   "Inert BackupRefPtr" _has_ overhead - once BRP support is compiled
    in, every `raw_ptr` will (at assignment) perform the
    check that asks, ["is BRP protection active?"][is-brp-active]

As for general BRP enablement,

*   BRP is live in most browser tests and Chromium targets.

    *   This is nuanced by platform type and process type.

*   In unit tests,

    *   `raw_ptr` is the no-op impl when the build is ASan.

    *   `raw_ptr` is live BRP on bots.

    *   `raw_ptr` is inert BRP otherwise (see https://crbug.com/1440658).

[raw-ptr-noop-impl]: https://source.chromium.org/search?q=class:RawPtrNoOpImpl&ss=chromium
[is-brp-active]: https://source.chromium.org/search?q=func:RawPtrBackupRefImpl::IsSupportedAndNotNull&ss=chromium
