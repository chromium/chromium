---
name: base/narrowly-scope-task-execution-context-overrides
rubric: correctness
severity: should_fix
---

**Rule.** In task scheduling and execution infrastructure (`base/task`), scope
RAII execution-context overrides (such as `CurrentTaskImportanceOverride` or
`ScopedSetTaskPriorityForCurrentThread`) to the narrowest possible scope
immediately around the task execution callback (such as
`task_annotator_.RunTask()`). Do not wrap surrounding scheduler machinery,
tracing macros, or feature initialization.

**Why.** Surrounding scheduler infrastructure—including tracing setup, feature
flag initialization, and sequence manager bookkeeping—can post tasks
unexpectedly. Scoping overrides too broadly causes internal tasks posted during
scheduler machinery to inadvertently inherit the executing task's importance or
priority.

## Examples

Bad:

```cpp
// Bad: Overriding task importance before tracing and instrumentation setup
base::internal::CurrentTaskImportanceOverride thread_type_override(
    selected_task->thread_type);

TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("devtools.timeline"), "RunTask");
{
  TaskAnnotator::LongTaskTracker long_task_tracker(...);
  task_annotator_.RunTask(...);
}
```

Good:

```cpp
// Good: Scope the override as narrowly as possible immediately around RunTask
TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("devtools.timeline"), "RunTask");
{
  TaskAnnotator::LongTaskTracker long_task_tracker(...);
  base::internal::CurrentTaskImportanceOverride thread_type_override(
      selected_task->thread_type);
  task_annotator_.RunTask(...);
}
```
