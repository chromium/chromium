---
name: general/explain-concrete-ownership-in-unretained-safety-comments
rubric: readability
severity: nit
---

**Rule.** When adding comments explaining why `base::Unretained(this)` or an
unowned raw pointer is safe (such as with `CallbackListSubscription`, observer
registrations, or thread joins), state the concrete ownership relationship or
cancellation mechanism (e.g., that the subscription is a member of `this` and
destroyed first, or that the worker thread is joined in the destructor) rather
than merely asserting the conclusion that the callback or subscription does not
outlive `this`.

**Why.** Merely asserting that a subscription or callback does not outlive an
object restates the safety requirement without justifying how that invariant is
guaranteed. Explaining the concrete ownership hierarchy (e.g., subscription
member destroyed before `this`) or cancellation point helps reviewers and
maintainers verify lifetime safety and prevent use-after-free bugs.

## Examples

Bad:

```cpp
// Unretained(this) is safe here since the CallbackListSubscription does not outlive |this|.
base::CallbackListSubscription subscription_ =
    source->Register(base::BindRepeating(&Foo::OnEvent, base::Unretained(this)));
```

Good:

```cpp
// Unretained(this) is safe here since `subscription_` is a member of `this`
// and is destroyed before `this` is destructed.
base::CallbackListSubscription subscription_ =
    source->Register(base::BindRepeating(&Foo::OnEvent, base::Unretained(this)));
```
