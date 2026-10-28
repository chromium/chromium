# Common Chromium Flakiness Patterns & Troubleshooting Strategies

Consult this reference on demand during Phase 2B of [SKILL.md](../SKILL.md) when
the root cause is not immediately obvious from the failure log or when a test
passes locally but fails in CQ's `check flakiness` step.

## 1. Unawaited Asynchronous Setup or Callbacks

- **Discarded `TestFuture` or Unawaited Callback**: Calling an asynchronous
  helper that returns a `base::test::TestFuture` (or takes a completion
  callback) without calling `Wait()` / `Get()` before triggering the next state
  transition in the test.
- **Partial Channel Synchronization**: Quitting a `base::RunLoop` when only one
  endpoint of a bidirectional channel or Mojo pipe has connected, before the
  peer's asynchronously posted open callback has fired. Register open callbacks
  for both endpoints and quit the loop only after both are ready.

## 2. Fixture Teardown & Member Destruction Ordering (LSan Leaks & `raw_ptr` UAFs)

- **Reverse Member Destruction Order (`TaskEnvironment`)**: In C++, class
  members are destroyed in **reverse** declaration order. If
  `base::test::TaskEnvironment` (or `SingleThreadTaskEnvironment`) is declared
  *after* members that post asynchronous destruction tasks (such as
  `base::SequenceBound<T>` or objects using `DeleteSoon`), the `TaskEnvironment`
  is destroyed first and drops the pending cleanup task, causing LeakSanitizer
  (`LSan`) leaks.
  - **Fix**: Declare `TaskEnvironment` as the **first** member variable of the
    test fixture, reset async members in `TearDown()`, and flush the target task
    runner before `TearDown()` finishes. Remove any `LeakedDanglingUntriaged`
    annotations once resolved.
- **Premature `TearDown()` Reset vs. Destructor Unregistration (`raw_ptr`
  UAF)**: When a test helper or client holds an unowned `raw_ptr<T>` to a
  fixture-owned resource and unregisters itself in its destructor (e.g.,
  `~Client() { resource_->RemoveObserver(this); }`), explicitly calling
  `resource_.reset()` in `TearDown()` without first resetting `client_` destroys
  the pointee before the fixture destructor runs `~Client()`, triggering a
  deterministic BackupRefPtr UAF (`Check failed: IsPointeeAlive(address)`).
  - **Fix**: Rely on C++ reverse member destruction order by declaring the
    underlying resource *before* the client and omitting manual `reset()` calls
    in `TearDown()` (or reset dependent clients before the resource).

## 3. Deterministic Time & Presubmit-Safe Task Flushing

- **Explicit Synchronization Over `RunUntilIdle()`**: Chromium presubmit
  discourages `RunUntilIdle()` because it can return before off-sequence tasks
  bounce back or mask hangs. When flushing a specific task runner or waiting on
  an event, post a `base::test::TestFuture<void>` callback to that task runner
  and `ASSERT_TRUE(future.Wait())` (or use an explicit
  `RunLoop::QuitClosure()`).
- **Simulated Time (`MOCK_TIME`) Caveats**: Prefer event-driven waiters
  (`TestFuture`, `WaitableEvent`, `CallbackHelper` in Java) or virtual time
  (`base::test::TaskEnvironment::TimeSource::MOCK_TIME`) over wall-clock sleeps
  or polling loops. Note that `MOCK_TIME` only controls the main thread and
  `base::ThreadPool` (not standalone `base::Thread` instances or real OS
  sockets).
- **Wall-Clock Delta Drift in `base::RepeatingTimer` Callbacks**: Test
  generators that compute work per timer tick from real wall-clock elapsed time
  (`base::TimeTicks::Now() - last_time_`) flake under CPU contention on bots:
  - **Early wakeups** (`elapsed < kInterval`) yield `0` steps via integer
    division while still overwriting `last_time_ = now`, silently losing elapsed
    time across ticks and causing timeouts.
  - **Late wakeups** after a scheduling stall generate a large burst in a single
    tick and can trigger producer stop conditions before asynchronous consumers
    finish processing.
  - **Fix**: Advance synthetic state by a deterministic constant per timer tick
    (or advance `last_time_ += steps * kInterval` to preserve sub-interval
    remainders).

## 4. Cross-Process Contention in CQ's `check flakiness` Step

- **Why It Happens**: Normal suite runs execute each test once, so sibling tests
  rarely collide on the same external resource simultaneously. When a test is
  added or re-enabled, CQ's `test new tests for flakiness` step
  (`check flakiness shard #0`) runs
  `--test-launcher-bot-mode --gtest_repeat=20 --test-launcher-retry-limit=0`,
  spawning parallel worker processes (typically `cpus / 2`) that all execute the
  **same test concurrently** in a shared environment (such as a single shared
  `Xvfb` `DISPLAY` on Linux).
- **Symptom**: The test passes normal suite runs and single-process
  `--gtest_repeat` runs, but fails in `check flakiness` when parallel worker
  processes contend on process-external shared state (e.g., X11
  `PRIMARY`/`CLIPBOARD` selections, fixed network ports, or fixed temp paths).
- **Local Reproduction**: On Linux, wrap the outer test launcher in
  `xvfb-run -a` so all parallel worker processes share a single `DISPLAY`
  (matching CQ's `testing/xvfb.py` invocation):
  ```sh
  xvfb-run -a ./<out_dir>/<test_target> \
    --test-launcher-bot-mode \
    --gtest_filter='<TestSuite>.<TestName>' \
    --gtest_repeat=20 \
    --test-launcher-retry-limit=0
  ```
- **Fix**: Isolate resources per process (`base::ScopedTempDir`, ephemeral
  ports). When the resource is inherently global to the shared display or OS
  (such as the X11 clipboard on a shared `Xvfb` server), serialize parallel test
  processes in `SetUp()` using an advisory file lock
  (`flock(lock_file_.GetPlatformFile(), LOCK_EX)` on POSIX) and declare
  `lock_file_` as the first fixture member so the lock is held until all test
  connections and clients are destroyed.

## 5. In-Process State Bleed (`@Batch` & Shared Singletons)

- **Shared Process State**: In Android instrumentation tests annotated with
  `@Batch(Batch.PER_CLASS)` or `@Batch(Batch.UNIT_TESTS)`, or C++ tests that
  mutate process-wide singletons, leftover UI state (unclosed dialogs,
  snackbars, tabs) or overridden globals from an earlier test can cause
  subsequent tests to flake. Use `ScopedFeatureList` / `ScopedCommandLine` and
  reset modified global state in `@After` / `TearDown()`.
