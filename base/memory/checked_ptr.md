# CheckedPtr

`CheckedPtr<T>` is a smart pointer that triggers a crash when dereferencing a
dangling pointer.  It is currently considered **experimental** - please don't
use it in production code just yet.
`CheckedPtr<T>` is part of the
[go/miracleptr](https://docs.google.com/document/d/1pnnOAIz_DMWDI4oIOFoMAqLnf_MZ2GsrJNb_dbQ3ZBg/edit?usp=sharing)
project.


## Examples of using CheckedPtr instead of raw pointers

`CheckedPtr<T>` can be used to replace raw pointer fields (aka member
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


### Runtime errors

TODO(bartekn): Document runtime errors encountered by BackupRefPtr
(they are more rare than for CheckedPtr2,
but runtime errors still exist for BackupRefPtr).

TODO(glazunov): One example is
accessing a class' CheckedPtr fields in its base class' constructor:
https://source.chromium.org/chromium/chromium/src/+/master:third_party/blink/renderer/platform/wtf/doubly_linked_list.h;drc=cce44dc1cb55c77f63f2ebec5e7015b8dc851c82;l=52
