---
name: general/enforce-pod-and-layout-invariants-with-static-assert
rubric: safety
severity: should_fix
---

**Rule.** When a struct or class has a POD, standard layout, triviality, or
binary ABI requirement (such as for C/OS interop or FFI), enforce the invariant
at compile time using static assertions (static_assert in C++, compile-time
assert! in Rust) rather than comments. Verify type traits, sizes, and field
offsets across FFI boundaries with comments identifying foreign counterpart
definitions, keeping internal members private via friend declarations. For
concrete types, place layout static_asserts in \*\_unittest.cc test files (or a
.cc file) rather than headers to avoid compilation overhead across translation
units; for class templates, assert within the header template definition.

**Why.** Relying solely on comments or unchecked layout assumptions across C/OS
APIs or FFI boundaries risks silent memory corruption and ABI breakage if
layouts diverge. Making internal fields public solely for standard layout breaks
encapsulation; compile-time assertions on offsetof, sizeof, and alignof verify
compatibility while preserving member privacy. Documenting foreign counterpart
types clarifies where to mirror updates, while placing concrete static
assertions in unit test files verifies invariants without imposing compilation
overhead across translation units.

## Examples

Bad:

```cpp
// In base/win/event_trace_provider.h (relying solely on comments for C API layout):
// EtwMofEventBase must remain a POD for C API interop.
template <size_t N>
struct EtwMofEventBase {
  EVENT_TRACE_HEADER header;
  std::array<MOF_FIELD, N> fields;
};

// In base/pickle.cc (standalone type trait assertion in production .cc):
static_assert(std::is_trivial_v<Pickle::Header>,
              "Header must be a trivial class");

// In base/feature.h (making internal member public and asserting ABI in header):
struct BASE_EXPORT Feature {
  const char* const name;
  const FeatureState default_state;
  std::atomic<uint32_t> cached_value;  // Made public solely for FFI!

  static void AssertABI() {
    static_assert(sizeof(Feature) == (sizeof(void*) == 8 ? 16 : 12));
    static_assert(offsetof(Feature, name) == 0);
  }
};
```

Good:

```cpp
// In base/win/event_trace_provider.h (template static_assert in header):
template <size_t N>
class EtwMofEvent : public EtwMofEventBase<N> {
 public:
  using Super = EtwMofEventBase<N>;
  static_assert(std::is_trivial_v<Super> && std::is_standard_layout_v<Super>,
                "EtwMofEventBase must be a POD type for C API interop.");
};

// In base/feature.h (internal member kept private with friend access):
struct BASE_EXPORT Feature {
  const char* const name;
  const FeatureState default_state;

 private:
  friend class FeatureList;
  friend class FeatureListTest;
  mutable std::atomic<uint32_t> cached_value;
};

// In base/pickle_unittest.cc or base/feature_list_unittest.cc (in unit tests):
static_assert(std::is_trivial_v<Pickle::Header>,
              "Header must be a trivial class");

// If any of these static asserts fail, update the Rust equivalent in base/feature.rs:
static_assert(sizeof(Feature) == (sizeof(void*) == 8 ? 16 : 12));
static_assert(alignof(Feature) == alignof(void*));
static_assert(offsetof(Feature, name) == 0);
static_assert(offsetof(Feature, default_state) == sizeof(void*));
static_assert(offsetof(Feature, cached_value) == sizeof(void*) + 4);

// In base/feature.rs (cross-language FFI layout verified at compile time):
const _: () = {
    assert!(std::mem::size_of::<Feature>()
        == if std::mem::size_of::<*const ()>() == 8 { 16 } else { 12 });
    assert!(std::mem::align_of::<Feature>() == std::mem::size_of::<*const ()>());
    assert!(std::mem::offset_of!(Feature, name) == 0);
    assert!(std::mem::offset_of!(Feature, default_state) == std::mem::size_of::<*const ()>());
    assert!(std::mem::offset_of!(Feature, cached_value) == std::mem::size_of::<*const ()>() + 4);
};
```
