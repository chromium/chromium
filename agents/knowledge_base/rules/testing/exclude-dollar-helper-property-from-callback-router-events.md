---
name: testing/exclude-dollar-helper-property-from-callback-router-events
rubric: correctness
severity: should_fix
---

**Rule.** When enumerating or dynamically selecting event properties on a Mojo
JavaScript CallbackRouter to invoke addListener, explicitly exclude or filter
out the internal "$" receiver helper property (e.g., using .subtracting(["$"])).
Do not treat "$" as a dispatchable event target.

**Why.** In Mojo JS bindings, every CallbackRouter contains event receiver
properties for interface methods alongside an internal "$" property holding the
CallbackRouterReceiverHelper. Attempting to call addListener on "$" throws a
runtime TypeError, which breaks test generation and degrades fuzzing execution.

## Examples

Bad:

```cpp
// Bad: Selecting any property on CallbackRouter can pick "$", which lacks addListener
let eventName = ILType.jsLockRequestCallbackRouter.properties.randomElement()!
let listenerHost = b.getProperty(eventName, of: router)
b.callMethod("addListener", on: listenerHost, withArgs: [callback])
```

Good:

```cpp
// Good: Filter out "$" (the CallbackRouterReceiverHelper) before picking an event target
guard let eventName = ILType.jsLockRequestCallbackRouter.properties
    .subtracting(["$"])
    .randomElement() else { return }
let listenerHost = b.getProperty(eventName, of: router)
b.callMethod("addListener", on: listenerHost, withArgs: [callback])
```
