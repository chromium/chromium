# MiraclePtr aka CheckedPtr`<T>` aka BackupRefPtr

Chrome's biggest security problem is a constant stream of exploitable (and
exploited) Use-after-Free (UaF) bugs. `MiraclePtr` is an unmbrella term for
algorithms based on smart-pointer-like wrappers, whose goal is to stop UaFs from
being exploitable, by turning them from security bugs to non-security crashes or
memory leaks. See
[go/miracleptr](https://docs.google.com/document/d/1pnnOAIz_DMWDI4oIOFoMAqLnf_MZ2GsrJNb_dbQ3ZBg/edit?usp=sharing)
for details.

`CheckedPtr<T>` is a smart-pointer-like templated class that wraps a raw
pointer, protecting it with one of the `MiraclePtr` algorithms from being
exploited via UaF. The class name came from the first algorithm that we
evaluated, and is sujbect to change. `BackupRefPtr` is one of the `MiraclePtr`
algorithms, based on reference counting, that disarms UaFs by quarantining
allocations that have known pointers. It was deemed the most promising one and
is the only one under consideration at the moment.
In the current world, `MiraclePtr`, `BackupRefPtr` and `CheckedPtr<T>` became
effectively synonyms.

`CheckedPtr<T>` is currently considered **experimental** - please don't
use it in production code just yet.

## Examples of using CheckedPtr instead of raw pointers

For performance reasons, currently we only consider `CheckedPtr<T>`
to replace raw pointer fields (aka member
variables).  For example, the following struct that uses raw pointers:

```cpp
struct Example {
  int* int_ptr;
  void* void_ptr;
  SomeClass* object_ptr;
  const SomeClass* ptr_to_const;
  SomeClass* const const_ptr;
};
```

Would look as follows when using `CheckedPtr<T>`:

```cpp
#include "base/memory/checked_ptr.h"

struct Example {
  CheckedPtr<int> int_ptr;
  CheckedPtr<void> void_ptr;
  CheckedPtr<SomeClass> object_ptr;
  CheckedPtr<const SomeClass> ptr_to_const;
  const CheckedPtr<SomeClass> const_ptr;
};
```

In most cases, only the type in the field declaration needs to change.
In particular, `CheckedPtr<T>` implements
`operator->`, `operator*` and other operators
that one expects from a raw pointer.
A handful of incompatible cases are described in the
"Incompatibilities with raw pointers" section below.


## Benefits and costs of CheckedPtr

TODO: Expand the raw notes below:
- Benefit = making UaF bugs non-exploitable
  - Need to explain how BackupRefPtr implementation
    poisons/zaps/quarantines the freed memory
    as long as a dangling CheckedPtr exists
  - Need to explain the scope of the protection
    - non-renderer process only (e.g. browser process, NetworkService process,
      GPU process, etc., but *not* renderer processes, utility processes, etc.)
    - most platforms (except iOS;  and 32-bit might also be out of scope)
    - only pointers to PartitionAlloc-managed memory (all heap
      allocations via `malloc` or `new` in Chrome, but not
      pointers to stack memory, etc.)
- Cost = performance hit
  - Point to preliminary performance results and A/B testing results
  - Explain how the performance hit affects mostly construction
    and destruction (e.g. dereferencing or comparison are not affected).


## Fields should use CheckedPtr rather than raw pointers

Eventually, once CheckedPtr is no longer **experimental**,
fields (aka member variables) in Chromium code
should use `CheckedPtr<SomeClass>` rather than raw pointers.

TODO: Expand the raw notes below:
- Chromium-only (V8, Skia, etc. excluded)
- Renderer-only code excluded for performance reasons (Blink,
  any code path with "/renderer/" substring).
- Fields-only
  (okay to use raw pointer variables, params, container elements, etc.)
- TODO: Explain how this will be eventually enforced (presubmit? clang plugin?).
  Explain how to opt-out (e.g. see "Incompatibilities with raw pointers"
  section below where some scenarios are inherently incompatible
  with CheckedPtr).


## Incompatibilities with raw pointers

In most cases, changing the type of a field
(or a variable, or a parameter, etc.)
from `SomeClass*` to `CheckedPtr<SomeClass>`
shouldn't require any additional changes - all
other usage of the pointer should continue to
compile and work as expected at runtime.

There are some corner-case scenarios however,
where `CheckedPtr<SomeClass>` is not compatible with a raw pointer.
Subsections below enumerate such scenarios
and offer guidance on how to work with them.
For a more in-depth treatment, please see the
["Limitations of CheckedPtr/BackupRefPtr"](https://docs.google.com/document/d/1HbtenxB_LyxNOFj52Ph9A6Wzb17PhXX2NGlsCZDCfL4/edit?usp=sharing)
document.

### Compile errors

#### Explicit `.get()` might be required

If a raw pointer is needed, but an implicit cast from
`CheckedPtr<SomeClass>` to `SomeClass*` doesn't work,
then the raw pointer needs to be obtained by explicitly
calling `.get()`.  Examples:

- `auto* raw_ptr_var = checked_ptr.get()`
  (`auto*` requires the initializer to be a raw pointer)
- `return condition ? raw_ptr : checked_ptr.get();`
  (ternary operator needs identical types in both branches)
- `base::WrapUniquePtr(checked_ptr.get());`
  (implicit cast doesn't kick in for arguments in templates)
- `printf("%p", checked_ptr.get());`
  (can't pass class type arguments to variadic functions)
- `reinterpret_cast<SomeClass*>(checked_ptr.get())`
  (`const_cast` and `reinterpret_cast` sometimes require their
  argument to be a raw pointer;  `static_cast` should "Just Work")

#### In-out arguments need to be refactored

Due to implementation difficulties,
`CheckedPtr` doesn't support an address-of operator.
This means that the following code will not compile:

```cpp
void GetSomeClassPtr(SomeClass** out_arg) {
  *out_arg = ...;
}

struct MyStruct {
  void Example() {
    GetSomeClassPtr(&checked_ptr_);  // <- won't compile
  }

  CheckedPtr<SomeClass> checked_ptr_;
};
```

The typical fix is to change the type of the out argument:

```cpp
void GetSomeClassPtr(CheckedPtr<SomeClass>* out_arg) {
  *out_arg = ...;
}
```

If `GetSomeClassPtr` can be invoked _both_ with raw pointers
and with `CheckedPtr`, then both overloads might be needed:

```cpp
void GetSomeClassPtr(SomeClass** out_arg) {
  *out_arg = ...;
}

void GetSomeClassPtr(CheckedPtr<SomeClass>* out_arg) {
  SomeClass* tmp = **out_arg;
  GetSomeClassPtr(&tmp);
  *out_arg = tmp;
}
```

#### Global scope

`-Wexit-time-destructors` disallows triggering custom destructors
when global variables are destroyed.
Since `CheckedPtr` has a custom destructor,
it cannot be used as a field of structs that are used as global variables.
If a pointer needs to be used in a global variable
(directly or indirectly - e.g. embedded in an array or struct),
then the only solution is avoiding `CheckedPtr`.

Build error:

```build
error: declaration requires an exit-time destructor
[-Werror,-Wexit-time-destructors]
```


#### No `constexpr` for non-null values

`constexpr` raw pointers can be initialized with pointers to string literals
or pointers to global variables.  Such initialization doesn't work for
CheckedPtr which doesn't have a `constexpr` constructor for non-null pointer
values.

If `constexpr`, non-null initialization is required, then the only solution is
avoiding `CheckedPtr`.

#### Unions

If any member of a union has a non-trivial destructor, then the union
will not have a destructor.  Because of this `CheckedPtr<T>` usually cannot be
used to replace the type of union members, because `CheckedPtr<T>` has
a non-trivial destructor.

Build error:

```build
error: attempt to use a deleted function
note: destructor of 'SomeUnion' is implicitly deleted because variant
field 'checked_ptr' has a non-trivial destructor
```


### Runtime errors

#### Invalid pointer assignment

It is unsafe to assign `CheckedPtr` a raw pointer to freed memory even if the
`CheckedPtr` instance is never dereferenced, i.e. the following snippet will
likely cause a crash:

```cpp
void* ptr = malloc();
free(ptr);
[...]
CheckedPtr<void> checked_ptr = ptr;
```

At the very least, nothing prevents the memory slot, which is additionally used
to store the `CheckedPtr` metadata, from being decommitted. Furthermore, the
code pattern might lead to free list corruptions and concurrency issues.

On the other hand, assigning a dangling `CheckedPtr` to another `CheckedPtr` is
supported because the slot is guaranteed to be kept alive. Therefore, a
`CheckedPtr` instance should be only assigned a valid raw pointer, `nullptr` or
another `CheckedPtr`. Note that pointers right past the end of an allocation
considered valid in C++.


#### Assignment via reinterpret_cast

`CheckedPtr` maintains an internal ref-count associated with the piece of memory
that it points to (see the `PartitionRefCount` class).  The assignment operator
of `CheckedPtr` takes care to update the ref-count as needed, but the ref-count
may become unbalanced if the `CheckedPtr` value is assigned to without going
through the assignment operator.  An unbalanced ref-count may lead to crashes or
memory leaks.

One way to execute such an incorrect assignment is `reinterpret_cast` of
a pointer to a `CheckedPtr`.  For example, see https://crbug.com/1154799
where the `reintepret_cast` is/was used in the `Extract` method
[here](https://source.chromium.org/chromium/chromium/src/+/master:device/fido/cbor_extract.h;l=318;drc=16f9768803e17c90901adce97b3153cfd39fdde2)).
Simplified example:

```cpp
CheckedPtr<int> checked_int_ptr;
int** ptr_to_raw_int_ptr = reinterpret_cast<int**>(&checked_int_ptr);

// Incorrect code: the assignment below won't update the ref-count internally
// maintained by CheckedPtr.
*ptr_to_raw_int_ptr = new int(123);
```

Another way is to `reinterpret_cast` a struct containing `CheckedPtr` fields.
For example, see https://crbug.com/1165613#c5 where `reinterpret_cast` was
used to treat a `buffer` of data as `FunctionInfo` struct (where
`interceptor_address` field might be a `CheckedPtr`). Simplified example:

```cpp
struct MyStruct {
  CheckedPtr<int> checked_int_ptr_;
};

void foo(void* buffer) {
  // During the assignment, parts of `buffer` will be interpreted as an
  // already initialized/constructed `CheckedPtr<int>` field.
  MyStruct* my_struct_ptr = reinterpret_cast<MyStruct*>(buffer);

  // The assignment below will try to decrement the ref-count of the old
  // pointee.  This may crash if the old pointer is pointing to a
  // PartitionAlloc-managed allocation that has a ref-count already set to 0.
  my_struct_ptr->checked_int_ptr_ = nullptr;
}
```

#### Fields order leading to dereferencing a destructed CheckedPtr

Fields are destructed in the reverse order of their declarations:

```cpp
    struct S {
      Bar bar_;  // Bar is destructed last.
      CheckedPtr<Foo> foo_ptr_;  // CheckedPtr (not Foo) is destructed first.
    };
```

If destructor of `Bar` has a pointer to `S`, then it may try to dereference
`s->foo_ptr_` after `CheckedPtr` has been already destructed.
In practice this will lead to a null dereference and a crash
(e.g. see https://crbug.com/1157988).

Note that this code pattern would have resulted in an Undefined Behavior,
even if `foo_ptr_` was a raw `Foo*` pointer (see the
[memory-safete-dev@ discussion](https://groups.google.com/a/chromium.org/g/memory-safety-dev/c/3sEmSnFc61I/m/Ng6PyqDiAAAJ)
for more details).

Possible solutions (in no particular order):
- Declare the `bar_` field as the very last field.
- Declare the `foo_` field (and other POD or raw-pointer-like fields)
  before any other fields.
- Avoid accessing `S` from the destructor of `Bar`
  (and in general, avoid doing significant work from destructors).

#### Non-PA allocation address space reuse

An address goes from the "outside GigaCage" state to "inside GigaCage" while a `CheckedPtr` is pointing at it.

```cpp
  CheckedPtr<void> checked_ptr = mmap([...]);
  munmap(checked_ptr); // must be safe to keep checked_ptr alive since it's not going to be dereferenced
  void* ptr = allocator.root()->Alloc(16, ""); // PA creates a new superpage, which is by coincidence around the address checked_ptr points to
  checked_ptr = nullptr;
```

When this happens, it is like we skipped an `AddRef()` and `Release()` may decrement a non-existent ref count field. There is not enough address space to avoid the reuse on 32-bit platforms. In theory, we could store whether `CheckedPtr` pointed to a non-PA allocation during initialization and, therefore, should act like a no-op pointer, but we don't have a single spare bit in 32-bit pointers.

#### Past-the-end pointers with non-PA allocations

If we increment a `CheckedPtr` pointing at a non-PA allocation until it points past the end of the allocation, that pointer may happen to be pointing at the beginning of a PA superpage. Advancing the pointer through `operator+=()` assumes that the pointer stays within an allocation. So when this happens, it is as if we skipped an `AddRef()`, and `Release()` may decrement a non-existent ref count field.

#### Pointers to address in another process

If `CheckedPtr` is used to store an address in another process. The same address could be used in PA for the current process. Resulting in CheckedPtr trying to increment the ref count that doesn't exist.

`sandbox::GetProcessBaseAddress()` was an example of a function that returns an address in another process as `void*`, resulting in this issue.

#### Other

TODO(bartekn): Document runtime errors encountered by BackupRefPtr
(they are more rare than for CheckedPtr2,
but runtime errors still exist for BackupRefPtr).

TODO(glazunov): One example is
accessing a class' CheckedPtr fields in its base class' constructor:
https://source.chromium.org/chromium/chromium/src/+/master:third_party/blink/renderer/platform/wtf/doubly_linked_list.h;drc=cce44dc1cb55c77f63f2ebec5e7015b8dc851c82;l=52
