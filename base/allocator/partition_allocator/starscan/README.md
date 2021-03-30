# StarScan: Heap scanning use-after-free prevention

C++ and other languages that rely on explicit memory management using `malloc()`
and `free()` are prone to memory corruptions and the resulting security issues.
The fundamental idea behind these heap scanning algorithms is to intercept an
underlying allocator and delay releasing of memory until the corresponding
memory block is provably unreachable from application code.

The basic ingredients for such algorithms are:
1.  *Quarantine*: When an object is deemed unused with a `free()` call, it is
    put into quarantine instead of being returned to the allocator. The object
    is not actually freed by the underlying allocator and cannot be used for
    future allocation requests until it is found that no pointers are pointing
    to the given memory block.
2.  *Scan*: When the quarantine reaches a certain quarantine limit (e.g. based
    on memory size of quarantine list entries), the quarantine scan is
    triggered. The scan iterates over the application memory and checks if
    references are pointing to quarantined memory. If objects in the quarantine
    are still referenced then they are kept in quarantine, if not they are
    flagged to be released.
3.  *Sweep*: All objects that are flagged to be released are actually returned
    to the underlying memory allocator.

[Heap scanning algorithms](http://bit.ly/conservative-heap-scan) come in
different flavors that offer different performance and security characteristics.

*Probabilistic conservative scan (PCScan)* (`pcscan.{h,cc}`) is one particular
kind of heap scanning  algorithm implemented on top of
[PartitionAlloc](../PartitionAlloc.md) with the following properties:

*   Memory blocks are scanned conservatively for pointers.
*   Scanning and sweeping are generally performed on a separate thread to
    maximize application performance.
*   Lazy safe points prohibit certain operations from modifying the memory graph
    and provide convenient entry points for scanning the stack.

PCScan is currently considered **experimental** - please do not use it in
production code just yet. It can be enabled in the following configurations via
`--enable-features` on builds that use PartitionAlloc as the
[main allocator](../../README.md):

*   `PartitionAllocPCScan`: All processes and all supporting partitions enable
    PCScan.
*   `PartitionAllocPCScanBrowserOnly`: Enables PCScan in the browser process
    for the default malloc partition.
