---
name: general/use-notify-allow-reentrancy-only-on-disallowed-reentrancy-li
rubric: readability
severity: should_fix
---

**Rule.** Use `base::ObserverList::NotifyAllowReentrancy` (or
`NotifyAllowReentrancyUntriaged`) only with observer lists configured to
prohibit reentrancy (`ObserverListReentrancyPolicy::kDisallowReentrancy`). On
observer lists that already permit reentrancy (`kAllowReentrancy` or
`kAllowReentrancyUntriaged`), use standard `Notify()`.

**Why.** `NotifyAllowReentrancy` serves as an explicit escape hatch for
individual notification dispatches on an `ObserverList` that otherwise forbids
reentrant iteration. Calling `NotifyAllowReentrancy` on an observer list
configured to allow reentrancy is redundant and misleading because `Notify()`
already allows reentrancy on such lists.

## Examples

Bad:

```cpp
// On an ObserverList configured to allow reentrancy (kAllowReentrancy):
base::ObserverList<Observer, /*check_empty=*/false,
                   base::ObserverListReentrancyPolicy::kAllowReentrancy>
    observers_;
observers_.NotifyAllowReentrancy(&Observer::OnSomethingChanged);
```

Good:

```cpp
// On an ObserverList configured to allow reentrancy:
observers_.Notify(&Observer::OnSomethingChanged);

// On an ObserverList configured with kDisallowReentrancy where OnSomethingChanged is safe to re-enter:
observers_.NotifyAllowReentrancy(&Observer::OnSomethingChanged);
```
