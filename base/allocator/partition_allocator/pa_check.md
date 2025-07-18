# PA_CHECK()

PartitionAlloc is not re-entrant. This means that when a crash happens inside PartitionAllocator, PA_CHECK() is intended not to be symbolized, because memory allocation is required to symbolize addresses. Inside PartitionAllocator, it is not allowed to use the same PartitionRoot for another memory operation, e.g. malloc() and free(). If malloc() is replaced with PartitionAllocator and the malloc() hits PA_CHECK() failure, the symbolization will also use the same PartitionRoot for malloc() and will cause another crash or hang.

## Symbolize unsymbolized addresses

To symbolize addresses, we can use `//tools/valgrind/asan/asan_symbolizer.py`. However we should be careful about the order of addresses, because the addresses are not sorted, e.g.

```
#0 (module1.so + address1)
#2 (module1.so + address2)
#3 (module1.so + address3)
#1 (module2.so + address4)
```

PA_CHECK() makes addresses relative addresses inside each module, but it does the following on linux (or Android):

- open /proc/self/maps.
- parse each line and get a module name and its address space.
- check whether the given addresses are in the address space or not. If in the address space, output the relative addresses with the module name. e.g. `base_unittests + 0x0023f8`

To avoid memory allocation, the code does not remember module names with their address spaces. Instead, it outputs the relative addresses as soon as modules, which contain the addresses, are found. On the other hand, `asan_symbolize.py` doesn’t keep `#[0-9]+`. E.g. `asan_symbolize.py` generates the following:

```
#0 module1.cc:line_number1
#1 module1.cc:line_number2 (was #2)
#2 module1.cc:line_number3 (was #3)
#3 module2.cc:line_number4 (was #1)
```

c.f. `//tools/valgrind/asan/third_party/asan_symbolize.py` sets `frame_no = 0` at `frameno_str == ‘0’` and increments `frame_no` by 1

If we want to symbolize the addresses manually, we can use `//third_party/llvm-build/Release+Asserts/bin/llvm-symbolizer`.

E.g.
```
llvm-symbolizer -e module1.so -f -C -i address1 address2 address3
```

If the module is very large, it takes much time to symbolize. It is better to provide multiple addresses for a symbolizer at once.

Instead of `llvm-symbolizer`, `addr2line` also works for the symbolization. However `addr2line` is much slower than `llvm-symbolizer`.

Regarding MacOS, we can use `atos` for symbolization.

E.g. if we have:
```
#0 (base_unittests+address1)
```
we will try:
```
atos -offset -inlineFrames -arch x86_64 -o base_unittests address1
```

If we have a minidump (e.g. `base_unittests`'s `mindump.dmp`), lldb is also useful for symbolization.

```
lldb -c minidump.dmp -f base_unittests
(lldb) bt
```

## Symbolize unsymbolized addresses shown in cq or try-bots

(googler-only) Regarding cq or tri, we can get binaries from `cas input`. We will open the “swarming task page” and follow the “Download inputs files into directory foo:” steps in the “Reproducing the task locally”.

## PA_NOTREACHED hit

Since PA_NOTREACHED() causes immediate crash and no stack frames are shown, the following makes it easier to debug PA_NOTREACHED():

`//base/allocator/partition_allocator/src/partition_alloc/partition_alloc_base/check.cc`
```
void RawCheckFailure(const char* message) {
  //RawLog(LOGGING_FATAL, message);
  //PA_IMMEDIATE_CRASH();
  PA_CHECK(false) << message;
}
```
