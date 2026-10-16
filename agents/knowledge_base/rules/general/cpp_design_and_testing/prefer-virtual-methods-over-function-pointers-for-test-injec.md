---
name: general/prefer-virtual-methods-over-function-pointers-for-test-injec
rubric: design
severity: nit
---

**Rule.** When designing a class seam to intercept, mock, or customize internal
or platform operations for testing, prefer defining a virtual method that test
subclasses or fixtures can override. Avoid storing function pointer members,
std::function callbacks, or exposing dedicated test injection setter methods
solely for test-only overrides.

**Why.** Storing member function pointers or callbacks and exposing test
injection methods adds unnecessary member state, constructor parameters, and
boilerplate to production classes. Using virtual methods leverages standard C++
polymorphism, keeps production class definitions lean, and avoids requiring
global static pointers or trampolines in tests.

## Examples

Bad:

```cpp
class ThreadTypeManager {
 public:
  explicit constexpr ThreadTypeManager(
      void (*set_current_thread_type_impl)(ThreadType, MessagePumpType) =
          &SetCurrentThreadTypeImpl)
      : set_current_thread_type_impl_(set_current_thread_type_impl) {}

  void SetThreadImplForTesting(
      void (*set_current_thread_type_impl)(ThreadType, MessagePumpType));

 private:
  void (*set_current_thread_type_impl_)(ThreadType, MessagePumpType);
};
```

Good:

```cpp
class ThreadTypeManager {
 public:
  ThreadTypeManager() = default;
  virtual ~ThreadTypeManager() = default;

 protected:
  virtual void SetCurrentThreadTypeImpl(ThreadType thread_type,
                                        MessagePumpType pump_type_hint);
};
```
