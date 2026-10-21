---
name: base/test-all-thread-pool-execution-modes
rubric: testing
severity: should_fix
---

**Rule.** When adding or modifying TaskTraits, thread priorities, or scheduling
behavior in base/task/thread_pool, ensure unit test coverage exercises all
supported TaskSourceExecutionModes—specifically including parallel execution
(PooledParallelTaskRunner) alongside sequenced and single-threaded runners.
Verify that tasks posted with the traits route to the expected ThreadGroup and
execute in the correct runtime thread environment (e.g., ThreadType or priority
via PlatformThread::GetCurrentThreadType()).

**Why.** Different task execution modes manage sequence lifecycles differently
(e.g., PooledParallelTaskRunner instantiates a one-off Sequence for each posted
task whereas sequenced runners reuse sequences). Parameterizing tests across all
execution modes and asserting the runtime thread environment ensures that trait
propagation, task routing to worker thread groups, and thread priority settings
function correctly across all task runner types.

## Examples

Bad:

```cpp
// Testing traits only on sequenced runners without verifying thread environment or parallel mode:
std::vector<TraitsExecutionModePair> GetTraitsExecutionModePairs() {
  // Only standard TaskPriority combinations tested, omitting InheritThreadType
  // or omitting PooledParallelTaskRunner execution modes.
}
```

Good:

```cpp
// Parameterize across all execution modes and verify ThreadGroup/ThreadType:
for (TaskSourceExecutionMode execution_mode : execution_modes) {
  params.push_back(
      TraitsExecutionModePair({InheritThreadType()}, execution_mode));
  params.push_back(TraitsExecutionModePair(
      {InheritThreadType(), MaxThreadType(ThreadType::kBackground)},
      execution_mode));
}

void VerifyTaskEnvironment(const TaskTraits& traits,
                           ThreadType originating_thread_type) {
  const bool expect_bg =
      TraitsSupportBackgroundThreadType(traits, originating_thread_type);
  EXPECT_EQ(expect_bg ? ThreadType::kBackground : ThreadType::kDefault,
            PlatformThread::GetCurrentThreadType());
}
```
