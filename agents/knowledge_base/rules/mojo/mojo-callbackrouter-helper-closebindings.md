---
name: mojo/mojo-callbackrouter-helper-closebindings
rubric: correctness
severity: should_fix
---

**Rule.** When defining Fuzzilli Mojo profile ILTypes, ObjectGroups, template
generators, or Mojo JavaScript bindings for CallbackRouter receiver helpers
('$'), include 'closeBindings' (returning .undefined) alongside
'associateAndPassRemote'. Note that while remote wrappers expose 'close()',
receiver helper objects expose 'closeBindings()' to tear down bound endpoints.

**Why.** In Mojo JavaScript bindings, remote wrappers provide 'close()' while
CallbackRouter receiver helpers ('$') expose 'closeBindings()' to close all
bound endpoints. Modeling 'closeBindings' in both ILType method lists and
ObjectGroup method tables enables fuzzers and test harnesses to exercise
endpoint teardown and disconnection during in-flight operations.

## Examples

Bad:

```cpp
fileprivate static let jsLockCallbackRouterReceiverHelper: ILType = .object(
    ofGroup: "LockCallbackRouterReceiverHelper",
    withMethods: ["associateAndPassRemote"])

private let lockCallbackRouterReceiverHelper = ObjectGroup(
    name: "LockCallbackRouterReceiverHelper",
    instanceType: .jsLockCallbackRouterReceiverHelper,
    properties: [:],
    methods: [
        "associateAndPassRemote": [] => .jsLockRemote
    ])
```

Good:

```cpp
fileprivate static let jsLockCallbackRouterReceiverHelper: ILType = .object(
    ofGroup: "LockCallbackRouterReceiverHelper",
    withMethods: ["associateAndPassRemote", "closeBindings"])

private let lockCallbackRouterReceiverHelper = ObjectGroup(
    name: "LockCallbackRouterReceiverHelper",
    instanceType: .jsLockCallbackRouterReceiverHelper,
    properties: [:],
    methods: [
        "associateAndPassRemote": [] => .jsLockRemote,
        "closeBindings": [] => .undefined,
    ])
```
