---
name: general/match-framework-component-suffixes-to-inheritance
rubric: readability
severity: should_fix
---

**Rule.** Do not suffix a Java class name with an Android framework component
name (such as BroadcastReceiver, Service, Activity, or ContentProvider) unless
the class directly extends that framework component. If the class only wraps,
registers, or manages an inner component instance, extend the framework class
directly or name the class after its semantic role (e.g., \*Status, \*Manager,
\*Notifier, \*Observer).

**Why.** Android framework component suffixes establish strong expectations
regarding class inheritance, lifecycle callbacks, and API contracts. Naming a
wrapper or manager class with a component suffix when it does not inherit from
that component misleads readers and callers about its true type and behavior.

## Examples

Bad:

```cpp
/** A {@link BroadcastReceiver} that listens for the screen off broadcast. */
public class ScreenOffBroadcastReceiver {
    private final BroadcastReceiver mScreenOffReceiver = new BroadcastReceiver() {
        @Override
        public void onReceive(Context context, Intent intent) { ... }
    };
}
```

Good:

```cpp
/** A {@link BroadcastReceiver} that listens for the screen off broadcast. */
public class ScreenOffBroadcastReceiver extends BroadcastReceiver {
    @Override
    public void onReceive(Context context, Intent intent) { ... }
}
```
