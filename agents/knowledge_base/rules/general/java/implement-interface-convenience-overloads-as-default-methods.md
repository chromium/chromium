---
name: general/implement-interface-convenience-overloads-as-default-methods
rubric: readability
severity: nit
---

**Rule.** In Java interfaces, implement convenience method overloads that
forward default or null arguments as default methods delegating to the fully
parameterized method, rather than declaring them as separate abstract methods.

**Why.** Declaring convenience overloads as abstract methods forces every
implementing class and test double to implement repetitive forwarding
boilerplate. Providing a default delegating implementation in the interface
centralizes default argument handling and lets implementors focus solely on the
canonical method.

## Examples

Bad:

```cpp
public interface TaskRunner extends Executor {
    void postDelayedTask(Runnable task, long delay);
    void postDelayedTask(Runnable task, long delay, @Nullable Location location);
}
```

Good:

```cpp
public interface TaskRunner extends Executor {
    default void postDelayedTask(Runnable task, long delay) {
        postDelayedTask(task, delay, null);
    }
    void postDelayedTask(Runnable task, long delay, @Nullable Location location);
}
```
