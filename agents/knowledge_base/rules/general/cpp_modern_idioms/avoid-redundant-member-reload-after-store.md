---
name: general/avoid-redundant-member-reload-after-store
rubric: readability
severity: should_fix
---

**Rule.** Do not redundantly reload a value from a member or atomic variable
immediately after storing or assigning to it when the source value is already
available in the local scope. Pass or consume the local variable or function
parameter directly in subsequent calls and assertions instead of reloading the
member.

**Why.** Reloading a member or atomic immediately after writing to it introduces
visual clutter, incurs unnecessary memory or atomic load operations, and
misleads readers into questioning whether the stored state could have been
modified concurrently. Using the existing local variable or argument clarifies
data flow and keeps operations concise.

## Examples

Bad:

```cpp
setup_.configurable_pool_base_address_.store(pool_base,
                                             std::memory_order_relaxed);
AddressPoolManager::GetInstance().Add(
    kConfigurablePoolHandle,
    setup_.configurable_pool_base_address_.load(std::memory_order_relaxed),
    size);
```

Good:

```cpp
setup_.StoreConfigurablePoolBaseAddress(pool_base);
AddressPoolManager::GetInstance().Add(kConfigurablePoolHandle, pool_base,
                                      size);
```
