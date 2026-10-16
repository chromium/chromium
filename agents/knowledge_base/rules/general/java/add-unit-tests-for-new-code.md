---
name: general/add-unit-tests-for-new-code
rubric: testing
severity: should_fix
---

**Rule.** Add unit and regression test coverage (e.g., GoogleTest in C++ or
Robolectric in Java) for newly introduced or refactored classes, methods,
accessors, constructor parameters, and bug fixes in the same changelist rather
than deferring tests. Structure classes for testability by parameterizing
dependencies or exposing test hooks (`ForTesting`) instead of relying on heavy
integration environments. When deleting obsolete internal helpers, ensure
surviving public APIs retain explicit direct unit tests. Ensure tests exercise
actual production entry points directly rather than isolated platform
techniques, verify state mutations, cover const and non-const overloads, and
explicitly instantiate and exercise modified C++ template member functions.

**Why.** Adding functionality, classes, constructor parameters, bug fixes, or
refactoring implementations without tests leaves behavior unverified and allows
silent regressions. In C++, uncalled member functions of class templates are
never instantiated by the compiler, leaving syntax errors and missing return
statements undetected. Testing isolated platform mechanisms fails to verify
production code execution, while direct tests against production entry points
and testable, parameterized interfaces ensure correct behavior without polluting
global state.

## Examples

Bad:

```cpp
// 1. Introducing a new class/component without test coverage or isolation hooks:
public final class ScopedBatchUpdate implements AutoCloseable {
  private static volatile @Nullable Handler sLauncherHandler;
  ...
}

// 2. Modifying template methods without test instantiation:
template <typename Traits>
class ScopedGeneric {
 public:
  Receiver& operator=(Receiver&& move) {
    scoped_generic_ = move.scoped_generic_;
    move.scoped_generic_ = nullptr;
    // Missing return statement is never caught by compiler if uninstantiated.
  }
};

// 3. Testing isolated OS technique rather than production entry point:
TEST(LaunchWinTest, ConoutOpenedWithReadWriteSupportsGetConsoleMode) {
  AllocConsole();
  FILE* conout = fopen("CONOUT$", "w+");
  HANDLE handle = reinterpret_cast<HANDLE>(_get_osfhandle(_fileno(conout)));
  DWORD mode;
  EXPECT_TRUE(GetConsoleMode(handle, &mode));
  fclose(conout);
}
```

Good:

```cpp
// 1. Class designed for testability with test hooks and accompanied by unit tests:
public final class ScopedBatchUpdate implements AutoCloseable {
  private static @Nullable BindingRequestQueue sBindingRequestQueueForTesting;

  public static void setBindingRequestQueueForTesting(@Nullable BindingRequestQueue queue) {
    sBindingRequestQueueForTesting = queue;
  }
}
// In ScopedBatchUpdateTest.java (Robolectric test):
@RunWith(BaseRobolectricTestRunner.class)
public class ScopedBatchUpdateTest {
  @Test
  public void testBatchUpdate() { ... }
}

// 2. Unit tests explicitly instantiating and exercising template methods and overloads:
TEST(ScopedGenericTest, ReceiverMoveAssign) {
  std::vector<int> values_freed;
  IntTraits traits(&values_freed);
  ScopedInt a(0, traits);
  ScopedInt b(0, traits);
  ScopedInt::Receiver r1(a);
  ScopedInt::Receiver r2(b);
  r2 = std::move(r1);
  *r2.get() = 42;
}

// 3. Direct unit test exercising production entry point (using child process or helper):
MULTIPROCESS_TEST_MAIN(RouteStdioToConsoleChild) {
  base::RouteStdioToConsole(/*create_console_if_not_found=*/true);
  HANDLE stdout_handle = reinterpret_cast<HANDLE>(_get_osfhandle(_fileno(stdout)));
  DWORD mode;
  return GetConsoleMode(stdout_handle, &mode) ? 0 : 2;
}
```
