# MiraclePtr aka raw_ptr aka BackupRefPtr

Chrome's biggest security problem is a constant stream of exploitable (and
exploited) Use-after-Free (UaF) bugs. `MiraclePtr` is an unmbrella term for
algorithms based on smart-pointer-like wrappers, whose goal is to stop UaFs from
being exploitable, by turning them from security bugs to non-security crashes or
memory leaks. See
[go/miracleptr](https://docs.google.com/document/d/1pnnOAIz_DMWDI4oIOFoMAqLnf_MZ2GsrJNb_dbQ3ZBg/edit?usp=sharing)
for details.

`raw_ptr<T>` (formerly `CheckedPtr<T>`) is a smart-pointer-like templated class
that wraps a raw pointer, protecting it with one of the `MiraclePtr` algorithms
from being exploited via UaF. The class name came from the first algorithm that
we evaluated, and is sujbect to change. `BackupRefPtr` is one of the
`MiraclePtr` algorithms, based on reference counting, that disarms UaFs by
quarantining allocations that have known pointers. It was deemed the most
promising one and is the only one under consideration at the moment.
In the current world, `MiraclePtr`, `BackupRefPtr` and `raw_ptr<T>` became
effectively synonyms.

`raw_ptr<T>` is currently considered **experimental** - please don't
use it in production code just yet.

## Examples of using raw_ptr instead of raw pointers

For performance reasons, currently we only consider `raw_ptr<T>`
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

Would look as follows when using `raw_ptr<T>`:

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
A handful of incompatible cases are described in the
"Incompatibilities with raw pointers" section below.


## Benefits and costs of raw_ptr

TODO: Expand the raw notes below:
- Benefit = making UaF bugs non-exploitable
  - Need to explain how BackupRefPtr implementation
    poisons/zaps/quarantines the freed memory
    as long as a dangling `raw_ptr<T>` exists
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


## Fields should use raw_ptr rather than raw pointers

Eventually, once `raw_ptr<T>` is no longer **experimental**,
fields (aka member variables) in Chromium code
should use `raw_ptr<SomeClass>` rather than raw pointers.

TODO: Expand the raw notes below:
- Chromium-only (V8, Skia, etc. excluded)
- Renderer-only code excluded for performance reasons (Blink,
  any code path with "/renderer/" substring).
- Fields-only
  (okay to use raw pointer variables, params, container elements, etc.)
- TODO: Explain how this will be eventually enforced (presubmit? clang plugin?).
  Explain how to opt-out (e.g. see "Incompatibilities with raw pointers"
  section below where some scenarios are inherently incompatible
  with `raw_ptr<T>`).


## Incompatibilities with raw pointers

In most cases, changing the type of a field
(or a variable, or a parameter, etc.)
from `SomeClass*` to `raw_ptr<SomeClass>`
shouldn't require any additional changes - all
other usage of the pointer should continue to
compile and work as expected at runtime.

There are some corner-case scenarios however,
where `raw_ptr<SomeClass>` is not compatible with a raw pointer.
Subsections below enumerate such scenarios
and offer guidance on how to work with them.
For a more in-depth explanation, please see the
["BackupRefPtr Support Coverage"](https://docs.google.com/document/d/1-H8zS4p2jKNo4Zsv2rbXcYvGKn2CsCTtd1W1HPl3z_M/edit?usp=sharing)
document.

### Compile errors

#### Explicit `.get()` might be required

If a raw pointer is needed, but an implicit cast from
`raw_ptr<SomeClass>` to `SomeClass*` doesn't work,
then the raw pointer needs to be obtained by explicitly
calling `.get()`.  Examples:

- `auto* raw_ptr_var = wrapped_ptr.get()`
  (`auto*` requires the initializer to be a raw pointer)
- `return condition ? raw_ptr : wrapped_ptr.get();`
  (ternary operator needs identical types in both branches)
- `base::WrapUniquePtr(wrapped_ptr.get());`
  (implicit cast doesn't kick in for arguments in templates)
- `printf("%p", wrapped_ptr.get());`
  (can't pass class type arguments to variadic functions)
- `reinterpret_cast<SomeClass*>(wrapped_ptr.get())`
  (`const_cast` and `reinterpret_cast` sometimes require their
  argument to be a raw pointer;  `static_cast` should "Just Work")

#### In-out arguments need to be refactored

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

If `GetSomeClassPtr` can be invoked _both_ with raw pointers
and with `raw_ptr<T>`, then both overloads might be needed:

```cpp
void GetSomeClassPtr(SomeClass** out_arg) {
  *out_arg = ...;
}

void GetSomeClassPtr(raw_ptr<SomeClass>* out_arg) {
  SomeClass* tmp = **out_arg;
  GetSomeClassPtr(&tmp);
  *out_arg = tmp;
}
```

#### Global scope

`-Wexit-time-destructors` disallows triggering custom destructors
when global variables are destroyed.
Since `raw_ptr<T>` has a custom destructor,
it cannot be used as a field of structs that are used as global variables.
If a pointer needs to be used in a global variable
(directly or indirectly - e.g. embedded in an array or struct),
then the only solution is avoiding `raw_ptr<T>`.

Build error:

```build
error: declaration requires an exit-time destructor
[-Werror,-Wexit-time-destructors]
```


#### No `constexpr` for non-null values

`constexpr` raw pointers can be initialized with pointers to string literals
or pointers to global variables.  Such initialization doesn't work for
`raw_ptr<T>` which doesn't have a `constexpr` constructor for non-null
pointer values.

If `constexpr`, non-null initialization is required, then the only solution is
avoiding `raw_ptr<T>`.

#### Unions

If any member of a union has a non-trivial destructor, then the union
will not have a destructor.  Because of this `raw_ptr<T>` usually cannot be
used to replace the type of union members, because `raw_ptr<T>` has
a non-trivial destructor.

Build error:

```build
error: attempt to use a deleted function
note: destructor of 'SomeUnion' is implicitly deleted because variant
field 'wrapped_ptr' has a non-trivial destructor
```


### Runtime errors

#### Invalid pointer assignment

It is unsafe to assign `raw_ptr<T>` a raw pointer to freed memory even if the
`raw_ptr<T>` instance is never dereferenced, i.e. the following snippet will
likely cause a crash:

```cpp
void* ptr = malloc();
free(ptr);
[...]
raw_ptr<void> wrapped_ptr = ptr;
```

At the very least, nothing prevents the memory slot, which is additionally used
to store the `raw_ptr<T>` metadata, from being decommitted. Furthermore, the
code pattern might lead to free list corruptions and concurrency issues.

On the other hand, assigning a dangling `raw_ptr<T>` to another `raw_ptr<T>` is
supported because the slot is guaranteed to be kept alive. Therefore, a
`raw_ptr<T>` instance should be only assigned a valid raw pointer, `nullptr` or
another `raw_ptr<T>`. Note that pointers right past the end of an allocation
considered valid in C++.


#### Assignment via reinterpret_cast

`raw_ptr<T>` maintains an internal ref-count associated with the piece of memory
that it points to (see the `PartitionRefCount` class).  The assignment operator
of `raw_ptr<T>` takes care to update the ref-count as needed, but the ref-count
may become unbalanced if the `raw_ptr<T>` value is assigned to without going
through the assignment operator.  An unbalanced ref-count may lead to crashes or
memory leaks.

One way to execute such an incorrect assignment is `reinterpret_cast` of
a pointer to a `raw_ptr<T>`.  For example, see https://crbug.com/1154799
where the `reintepret_cast` is/was used in the `Extract` method
[here](https://source.chromium.org/chromium/chromium/src/+/main:device/fido/cbor_extract.h;l=318;drc=16f9768803e17c90901adce97b3153cfd39fdde2)).
Simplified example:

```cpp
raw_ptr<int> wrapped_ptr;
int** ptr_to_raw_int_ptr = reinterpret_cast<int**>(&wrapped_ptr);

// Incorrect code: the assignment below won't update the ref-count internally
// maintained by `wrapped_ptr`.
*ptr_to_raw_int_ptr = new int(123);
```

Another way is to `reinterpret_cast` a struct containing `raw_ptr<T>` fields.
For example, see https://crbug.com/1165613#c5 where `reinterpret_cast` was
used to treat a `buffer` of data as `FunctionInfo` struct (where
`interceptor_address` field might be a `raw_ptr<T>`). Simplified example:

```cpp
struct MyStruct {
  raw_ptr<int> checked_int_ptr_;
};

void foo(void* buffer) {
  // During the assignment, parts of `buffer` will be interpreted as an
  // already initialized/constructed `raw_ptr<int>` field.
  MyStruct* my_struct_ptr = reinterpret_cast<MyStruct*>(buffer);

  // The assignment below will try to decrement the ref-count of the old
  // pointee.  This may crash if the old pointer is pointing to a
  // PartitionAlloc-managed allocation that has a ref-count already set to 0.
  my_struct_ptr->checked_int_ptr_ = nullptr;
}
```

#### Fields order leading to dereferencing a destructed raw_ptr

Fields are destructed in the reverse order of their declarations:

```cpp
    struct S {
      Bar bar_;  // Bar is destructed last.
      raw_ptr<Foo> foo_ptr_;  // raw_ptr<Foo> (not Foo) is destructed first.
    };
```

If destructor of `Bar` has a pointer to `S`, then it may try to dereference
`s->foo_ptr_` after `raw_ptr<T>` has been already destructed.
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

#### Past-the-end pointers with non-PA allocations

Pointers past the end of an allocation are supported only if they point exactly to the end of the allocation. Anything beyond that runs into a risk of modifying ref-count of the next allocation, or in the rare case, confusing the ref-counting logic entirely when an allocation is on the border of GigaCage. This could lead to obscure, hard to debug crashes.

#### Pointers to address in another process

If `raw_ptr<T>` is used to store an address in another process. The same address could be used in PA for the current process. Resulting in `raw_ptr<T>` trying to increment the ref count that doesn't exist.

`sandbox::GetProcessBaseAddress()` was an example of a function that returns an address in another process as `void*`, resulting in this issue.

#### Other

TODO(bartekn): Document runtime errors encountered by BackupRefPtr.

TODO(glazunov): One example is
accessing a class' `raw_ptr<T>` fields in its base class' constructor:
https://source.chromium.org/chromium/chromium/src/+/main:third_party/blink/renderer/platform/wtf/doubly_linked_list.h;drc=cce44dc1cb55c77f63f2ebec5e7015b8dc851c82;l=52
