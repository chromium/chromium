---
name: general/register-exit-handlers-at-entrypoint-not-static-initializers
rubric: correctness
severity: should_fix
---

**Rule.** Do not register exit handlers (such as atexit(), at_quick_exit(), or
base::AtExitManager callbacks) inside static global initializers or constructors
when attempting to preempt teardown, destructors, or runtime shutdown hooks.
Because exit handlers execute in LIFO (last-in, first-out) order and static
initialization order is undefined, register preemption handlers as late as
possible (e.g., directly in main() or the test runner entrypoint) so they
execute first upon process termination.

**Why.** Exit handlers execute in reverse order of registration (LIFO). Handlers
registered during static initialization run after subsequently registered
handlers—such as runtime cleanups, JNI teardown, or static destructors—defeating
preemption. Registering at the main or test runner entrypoint guarantees the
exit handler runs first upon exit.

## Examples

Bad:

```cpp
// Bad: Static constructor registers an exit handler during early startup.
struct FuzzerCleanExit {
  FuzzerCleanExit() {
    atexit([]() { _exit(0); });
  }
} g_fuzzer_clean_exit;
```

Good:

```cpp
// Good: Register the exit handler late at the runner entrypoint so it runs first.
int Main(int argc, char** argv) {
#if BUILDFLAG(USE_FUZZING_ENGINE)
  atexit([]() { _exit(0); });
#endif
  return RunTests(argc, argv);
}
```
