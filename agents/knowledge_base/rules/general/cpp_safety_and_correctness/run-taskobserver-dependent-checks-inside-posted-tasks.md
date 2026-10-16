---
name: general/run-taskobserver-dependent-checks-inside-posted-tasks
rubric: correctness
severity: should_fix
---

**Rule.** When testing state or features whose active lifecycle is monitored or
gated by a base::TaskObserver, execute assertions inside a dispatched task (e.g.
via PostTask and RunLoop) or disable the task observer feature in the test
fixture. Because GoogleTest test bodies execute synchronously on the runner
thread outside base::PendingTask dispatch, task observers treat the test body as
idle.

**Why.** base::TaskObserver receives WillProcessTask and DidProcessTask
notifications only during sequence manager task execution. In synchronous test
bodies, task observers see zero active tasks and leave task-gated features
inactive, causing tests to observe inactive state unless executed within a
queued task.

## Examples

Bad:

```cpp
// Fails because IN_PROC_BROWSER_TEST_F test bodies execute outside base::PendingTask dispatch;
// TaskObserver sees zero active tasks and treats quarantine as inactive.
IN_PROC_BROWSER_TEST_F(MyBrowserTest, FeatureActive) {
  EXPECT_TRUE(IsQuarantineActive());
}
```

Good:

```cpp
// Option 1: Disable the task observer feature in the test fixture.
feature_list_.InitWithFeatures(
    /*enabled_features=*/{kMyFeature},
    /*disabled_features=*/{
        features::kPartitionAllocSchedulerLoopQuarantineTaskObserverForBrowserUIThread});

// Option 2: Execute assertions inside a posted task where TaskObserver is active.
base::RunLoop run_loop;
base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
    FROM_HERE, base::BindLambdaForTesting([&]() {
      EXPECT_TRUE(IsQuarantineActive());
      run_loop.Quit();
    }));
run_loop.Run();
```
