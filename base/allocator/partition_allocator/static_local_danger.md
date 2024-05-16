# Dangerous Static Locals

Chromium style's rules regarding
[static and global variables][static-and-global]
make the following idiom attractive:

```c++
// This is
// * thread-safe (since C++11). Initialization happens at most once.
// * compliant with Chromium style, as destructor never runs.
NonTrivialFoo& GetFoo() {
  static base::NoDestructor<NonTrivialFoo> local;
  return *local;
}
```

However, the Windows C Runtime (CRT) forces the PartitionAlloc team to
use this idiom with great care. This is because initializing a
(nontrivial) function-local static object requires taking a lock
(entering a critical section).

![Windows CRT initialization is intercepted by PartitionAlloc, which a
  ttempts to initialize a function-local static, which causes re-entry i
  nto the uninitialized Windows CRT, forcing a c
  rash.](./src/partition_alloc/dot/windows_crt_static_local_ouroboros.png)

Therefore, it's imperative that control flow never passes into
initializing a static local before Windows CRT is fully initialized.

***aside
In particular, PartitionAlloc initialization during the first allocation
must not use these static values.
***

*** note
`constinit` and `constexpr` static locals are exempt from this rule.
Since they are initialized by the compiler, there's no dangerous
interaction with Windows CRT.
***

## Useful Reference Material

N.B. the first few items in this list (allocator shim and `ThreadCache`)
demonstrate successful workarounds for this puzzle.

1.  [These Google-internal slides][google-internal-pae-talk] describe
    the time we hit this when implementing PartitionAlloc-Everywhere.

1.  [This bug][pae-win-bug] was the background for the slides above
    and additionally chronicles other function-local statics rooted
    out to get PA-E working on Windows.

1.  [We hit this again in 2020][thread-cache-issue]
    in `ThreadCache` initialization.

1.  [We hit this again in 2022][address-pool-manager-issue] in
    attempting to change `AddressPoolManager` initialization. This was
    later [fixed without using a
    static local][address-pool-manager-workaround].

1.  [We hit this again in 2024][freelist-dispatcher-issue] in
    trying to set up a freelist experiment.


[static-and-global]: https://google.github.io/styleguide/cppguide.html#Static_and_Global_Variables
[google-internal-pae-talk]: https://docs.google.com/presentation/d/1fAgGNxaWtRbrTOJrNS9QG3nI6_c6efdW4RuzNtmKS1E/edit#slide=id.gdddf5d4640_0_364
[pae-win-bug]: https://issues.chromium.org/u/1/issues/40148130
[thread-cache-issue]: https://crrev.com/c/2474857
[address-pool-manager-issue]: https://crrev.com/c/3614389
[address-pool-manager-workaround]: https://crrev.com/c/3642766
[freelist-dispatcher-issue]: https://crbug.com/336007395
