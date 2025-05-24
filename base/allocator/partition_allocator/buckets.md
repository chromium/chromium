# Bucket Distribution in PartitionAlloc

In PartitionAlloc, a slab-based memory allocator, a "bucket" serves to
categorize different memory allocation sizes. When memory is requested,
PartitionAlloc rounds the requested size up to the nearest predefined size class
(referred to as slot size). Allocations that map to the same bucket are then
grouped together and allocated from a size-segregated slot span.

A bucket, therefore, defines a specific size category. This bucketing strategy
is key to how PartitionAlloc manages and organizes memory efficiently, offering
several benefits:

-   Increased cache locality for same-size allocations
-   Smaller metadata
-   Easy mapping of address to size
-   Decreased fragmentation over time

This document describes PartitionAlloc's methodology for mapping requested
allocation sizes to their corresponding buckets. See
`//partition_alloc/bucket_lookup.h` for implementation details.

## Bucket Distribution Types

PartitionAlloc provides [two different distributions](https://source.chromium.org/chromium/chromium/src/+/main:base/allocator/partition_allocator/src/partition_alloc/partition_root.h;l=238;drc=b3b10b6e91991505faa738b47ad263534341e05d);
Neutral and Denser.
As the name tells, Denser offers a more granular set of buckets, roughly
doubling the number compared to the Neutral distribution.

1. **Neutral Bucket Distribution** (`kNeutral`)
  * **Pro:** Results in fewer partially-filled slot spans, potentially reducing fragmentation caused by unused slots in these spans.
  * **Con:** Allocations are often rounded up to a significantly larger slot
  size than requested. This increases fragmentation *within* each allocation due
  to the larger difference between the requested size and the allocated slot
  size.
2. **Denser Bucket Distribution** (`kDenser`):
  * **Pro:** Allocations can more closely match the requested memory size.
  This reduces fragmentation *within* each allocation, as the chosen slot size
  is nearer to the actual need.
  * **Con:** May lead to more partially-filled slot spans. If these slot spans
  are not fully utilized, it can increase fragmentation due to more unused slots
  across these spans.

The Neutral distribution is implemented as a variation of the Denser one, making
it straightforward to understand if one understands the Denser layout.


## Denser Bucket Distribution: A Closer Look

The Denser distribution itself operates as a hybrid system. For smaller
allocation sizes, bucket sizes increase in a simple, linear fashion. Conversely,
for larger allocation sizes, the bucket sizes increase exponentially.

### Linear Sizing (for Smaller Allocations)

When an allocation request is for a relatively small amount of memory, the
system employs a linear scale. This means each subsequent bucket size is larger
than the previous one by a fixed increment. This increment is determined by the
system's fundamental memory alignment requirement, which might be, for instance,
16 bytes. As an example, if this fixed increment is 16 bytes, the sequence of
buckets might represent sizes such as 16 bytes, 32 bytes, 48 bytes, and so on.

### Exponential Sizing (for Larger Allocations)

For larger memory requests, the bucket sizes adhere to an exponential pattern.
The system divides broad power-of-two ranges, termed "orders," into a fixed
number of smaller bucket steps. For instance, the range of sizes from 128 bytes
up to, but not including, 256 bytes constitutes an "order," and it would contain
a specific number of distinct bucket sizes. The subsequent order, such as 256 to
511 bytes, would be similarly divided.

A fixed number of buckets, for example eight, are used to subdivide each
power-of-two range, creating what is known as "Buckets per Order." This
configuration results in a logarithmic scale where bucket sizes grow
proportionally rather than by a fixed additive amount. To illustrate with an
example using 8 buckets per order, sizes just above 128 bytes might be 128
bytes, then approximately 1.125x128 bytes, 1.25x128 bytes, and continue in this
manner up to nearly 256 bytes. This pattern then repeats for sizes above 256
bytes, then 512 bytes, and so forth.

||Order-Index 0|Order-Index 1|Order-Index 2|Order-Index 3|Order-Index 4|Order-Index 5|Order-Index 6|Order-Index 7|
|-:|:-:|:-:|:-:|:-:|:-:|:-:|:-:|:-:|
|Order  8 (2⁷)|121-128|129-144|145-160|161-176|177-192|193-208|209-224|225-240|
|Order  9 (2⁸)|241-256|257-288|289-320|321-352|353-384|385-416|417-448|449-480|
|Order 10 (2⁹)|481-512|513-576|577-640|641-704|705-768|769-832|833-896|897-960|

## Neutral Bucket Distribution

The Neutral Bucket Distribution offers a sparser alternative, derived from the
Denser one. In the range where the Denser distribution uses linear sizing, or
for the smallest exponential sizes where alignment naturally limits bucket
density, the Neutral and Denser distributions are identical. However, for larger
sizes within the exponential sizing range, the Neutral distribution typically
uses fewer buckets per "order" compared to the Denser one. It selects every
other bucket that the Denser distribution would define, leading to fewer, more
widely spaced buckets.

Consider an illustrative conceptual difference: if the Denser distribution has
buckets for sizes like ..., 384, 416, 448, 480, 512, ..., the Neutral
distribution, in the same range, might only have buckets for ..., 384, then skip
416 to use 448, then skip 480 to use 512, and so on.

## Example Distribution

### 8 Bytes Alignment (Typically 32-bit Systems)

| Index | Size | Bucket Distribution | Originating Formula |
| -: | -: | :- | :- |
|   0 |      8 | `kNeutral` and `kDenser` | linear [8 x 1] |
|   1 |     16 | `kNeutral` and `kDenser` | linear [8 x 2] |
|   2 |     24 | `kNeutral` and `kDenser` | linear [8 x 3] |
|   3 |     32 | `kNeutral` and `kDenser` | linear [8 x 4] |
|   4 |     40 | `kNeutral` and `kDenser` | linear [8 x 5] |
|   5 |     48 | `kNeutral` and `kDenser` | linear [8 x 6] |
|   6 |     56 | `kNeutral` and `kDenser` | linear [8 x 7] |
|   7 |     64 | `kNeutral` and `kDenser` | linear [8 x 8] yet exponential [2⁶ x (1 + 0)] |
|   8 |     72 | `kNeutral` and `kDenser` | linear [8 x 9] yet exponential [2⁶ x (1 + ⅛)] |
|   9 |     80 | `kNeutral` and `kDenser` | linear [8 x 10] yet exponential [2⁶ x (1 + ¼)] |
|  10 |     88 | `kNeutral` and `kDenser` | linear [8 x 11] yet exponential [2⁶ x (1 + ⅜)] |
|  11 |     96 | `kNeutral` and `kDenser` | linear [8 x 12] yet exponential [2⁶ x (1 + ½)] |
|  12 |    104 | `kNeutral` and `kDenser` | linear [8 x 13] yet exponential [2⁶ x (1 + ⅝)] |
|  13 |    112 | `kNeutral` and `kDenser` | linear [8 x 14] yet exponential [2⁶ x (1 + ¾)] |
|  14 |    120 | `kNeutral` and `kDenser` | linear [8 x 15] yet exponential [2⁶ x (1 + ⅞)] |
|  15 |    128 | `kNeutral` and `kDenser` | linear [8 x 16] yet exponential [2⁷ x (1 + 0)] |
|  16 |    144 | `kDenser` only           | exponential [2⁷ x (1 + ⅛)] |
|  17 |    160 | `kNeutral` and `kDenser` | exponential [2⁷ x (1 + ¼)] |
|  18 |    176 | `kDenser` only           | exponential [2⁷ x (1 + ⅜)] |
|  19 |    192 | `kNeutral` and `kDenser` | exponential [2⁷ x (1 + ½)] |
|  20 |    208 | `kDenser` only           | exponential [2⁷ x (1 + ⅝)] |
|  21 |    224 | `kNeutral` and `kDenser` | exponential [2⁷ x (1 + ¾)] |
|  22 |    240 | `kDenser` only           | exponential [2⁷ x (1 + ⅞)] |
|  23 |    256 | `kNeutral` and `kDenser` | exponential [2⁸ x (1 + 0)] |
|  24 |    288 | `kDenser` only           | exponential [2⁸ x (1 + ⅛)] |
|  25 |    320 | `kNeutral` and `kDenser` | exponential [2⁸ x (1 + ¼)] |
|  26 |    352 | `kDenser` only           | exponential [2⁸ x (1 + ⅜)] |
|  27 |    384 | `kNeutral` and `kDenser` | exponential [2⁸ x (1 + ½)] |
|  28 |    416 | `kDenser` only           | exponential [2⁸ x (1 + ⅝)] |
|  29 |    448 | `kNeutral` and `kDenser` | exponential [2⁸ x (1 + ¾)] |
|  30 |    480 | `kDenser` only           | exponential [2⁸ x (1 + ⅞)] |
|  31 |    512 | `kNeutral` and `kDenser` | exponential [2⁹ x (1 + 0)] |
|  32 |    576 | `kDenser` only           | exponential [2⁹ x (1 + ⅛)] |
|  33 |    640 | `kNeutral` and `kDenser` | exponential [2⁹ x (1 + ¼)] |
|  34 |    704 | `kDenser` only           | exponential [2⁹ x (1 + ⅜)] |
|  35 |    768 | `kNeutral` and `kDenser` | exponential [2⁹ x (1 + ½)] |
|  36 |    832 | `kDenser` only           | exponential [2⁹ x (1 + ⅝)] |
|  37 |    896 | `kNeutral` and `kDenser` | exponential [2⁹ x (1 + ¾)] |
|  38 |    960 | `kDenser` only           | exponential [2⁹ x (1 + ⅞)] |
|  39 |   1024 | `kNeutral` and `kDenser` | exponential [2¹⁰ x (1 + 0)] |
|  40 |   1152 | `kDenser` only           | exponential [2¹⁰ x (1 + ⅛)] |
|  41 |   1280 | `kNeutral` and `kDenser` | exponential [2¹⁰ x (1 + ¼)] |
|  42 |   1408 | `kDenser` only           | exponential [2¹⁰ x (1 + ⅜)] |
|  43 |   1536 | `kNeutral` and `kDenser` | exponential [2¹⁰ x (1 + ½)] |
|  44 |   1664 | `kDenser` only           | exponential [2¹⁰ x (1 + ⅝)] |
|  45 |   1792 | `kNeutral` and `kDenser` | exponential [2¹⁰ x (1 + ¾)] |
|  46 |   1920 | `kDenser` only           | exponential [2¹⁰ x (1 + ⅞)] |
|  47 |   2048 | `kNeutral` and `kDenser` | exponential [2¹¹ x (1 + 0)] |
|  48 |   2304 | `kDenser` only           | exponential [2¹¹ x (1 + ⅛)] |
|  49 |   2560 | `kNeutral` and `kDenser` | exponential [2¹¹ x (1 + ¼)] |
|  50 |   2816 | `kDenser` only           | exponential [2¹¹ x (1 + ⅜)] |
|  51 |   3072 | `kNeutral` and `kDenser` | exponential [2¹¹ x (1 + ½)] |
|  52 |   3328 | `kDenser` only           | exponential [2¹¹ x (1 + ⅝)] |
|  53 |   3584 | `kNeutral` and `kDenser` | exponential [2¹¹ x (1 + ¾)] |
|  54 |   3840 | `kDenser` only           | exponential [2¹¹ x (1 + ⅞)] |
|  55 |   4096 | `kNeutral` and `kDenser` | exponential [2¹² x (1 + 0)] |
|  56 |   4608 | `kDenser` only           | exponential [2¹² x (1 + ⅛)] |
|  57 |   5120 | `kNeutral` and `kDenser` | exponential [2¹² x (1 + ¼)] |
|  58 |   5632 | `kDenser` only           | exponential [2¹² x (1 + ⅜)] |
|  59 |   6144 | `kNeutral` and `kDenser` | exponential [2¹² x (1 + ½)] |
|  60 |   6656 | `kDenser` only           | exponential [2¹² x (1 + ⅝)] |
|  61 |   7168 | `kNeutral` and `kDenser` | exponential [2¹² x (1 + ¾)] |
|  62 |   7680 | `kDenser` only           | exponential [2¹² x (1 + ⅞)] |
|  63 |   8192 | `kNeutral` and `kDenser` | exponential [2¹³ x (1 + 0)] |
|  64 |   9216 | `kDenser` only           | exponential [2¹³ x (1 + ⅛)] |
|  65 |  10240 | `kNeutral` and `kDenser` | exponential [2¹³ x (1 + ¼)] |
|  66 |  11264 | `kDenser` only           | exponential [2¹³ x (1 + ⅜)] |
|  67 |  12288 | `kNeutral` and `kDenser` | exponential [2¹³ x (1 + ½)] |
|  68 |  13312 | `kDenser` only           | exponential [2¹³ x (1 + ⅝)] |
|  69 |  14336 | `kNeutral` and `kDenser` | exponential [2¹³ x (1 + ¾)] |
|  70 |  15360 | `kDenser` only           | exponential [2¹³ x (1 + ⅞)] |
|  71 |  16384 | `kNeutral` and `kDenser` | exponential [2¹⁴ x (1 + 0)] |
|  72 |  18432 | `kDenser` only           | exponential [2¹⁴ x (1 + ⅛)] |
|  73 |  20480 | `kNeutral` and `kDenser` | exponential [2¹⁴ x (1 + ¼)] |
|  74 |  22528 | `kDenser` only           | exponential [2¹⁴ x (1 + ⅜)] |
|  75 |  24576 | `kNeutral` and `kDenser` | exponential [2¹⁴ x (1 + ½)] |
|  76 |  26624 | `kDenser` only           | exponential [2¹⁴ x (1 + ⅝)] |
|  77 |  28672 | `kNeutral` and `kDenser` | exponential [2¹⁴ x (1 + ¾)] |
|  78 |  30720 | `kDenser` only           | exponential [2¹⁴ x (1 + ⅞)] |
|  79 |  32768 | `kNeutral` and `kDenser` | exponential [2¹⁵ x (1 + 0)] |
|  80 |  36864 | `kDenser` only           | exponential [2¹⁵ x (1 + ⅛)] |
|  81 |  40960 | `kNeutral` and `kDenser` | exponential [2¹⁵ x (1 + ¼)] |
|  82 |  45056 | `kDenser` only           | exponential [2¹⁵ x (1 + ⅜)] |
|  83 |  49152 | `kNeutral` and `kDenser` | exponential [2¹⁵ x (1 + ½)] |
|  84 |  53248 | `kDenser` only           | exponential [2¹⁵ x (1 + ⅝)] |
|  85 |  57344 | `kNeutral` and `kDenser` | exponential [2¹⁵ x (1 + ¾)] |
|  86 |  61440 | `kDenser` only           | exponential [2¹⁵ x (1 + ⅞)] |
|  87 |  65536 | `kNeutral` and `kDenser` | exponential [2¹⁶ x (1 + 0)] |
|  88 |  73728 | `kDenser` only           | exponential [2¹⁶ x (1 + ⅛)] |
|  89 |  81920 | `kNeutral` and `kDenser` | exponential [2¹⁶ x (1 + ¼)] |
|  90 |  90112 | `kDenser` only           | exponential [2¹⁶ x (1 + ⅜)] |
|  91 |  98304 | `kNeutral` and `kDenser` | exponential [2¹⁶ x (1 + ½)] |
|  92 | 106496 | `kDenser` only           | exponential [2¹⁶ x (1 + ⅝)] |
|  93 | 114688 | `kNeutral` and `kDenser` | exponential [2¹⁶ x (1 + ¾)] |
|  94 | 122880 | `kDenser` only           | exponential [2¹⁶ x (1 + ⅞)] |
|  95 | 131072 | `kNeutral` and `kDenser` | exponential [2¹⁷ x (1 + 0)] |
|  96 | 147456 | `kDenser` only           | exponential [2¹⁷ x (1 + ⅛)] |
|  97 | 163840 | `kNeutral` and `kDenser` | exponential [2¹⁷ x (1 + ¼)] |
|  98 | 180224 | `kDenser` only           | exponential [2¹⁷ x (1 + ⅜)] |
|  99 | 196608 | `kNeutral` and `kDenser` | exponential [2¹⁷ x (1 + ½)] |
| 100 | 212992 | `kDenser` only           | exponential [2¹⁷ x (1 + ⅝)] |
| 101 | 229376 | `kNeutral` and `kDenser` | exponential [2¹⁷ x (1 + ¾)] |
| 102 | 245760 | `kDenser` only           | exponential [2¹⁷ x (1 + ⅞)] |
| 103 | 262144 | `kNeutral` and `kDenser` | exponential [2¹⁸ x (1 + 0)] |
| 104 | 294912 | `kDenser` only           | exponential [2¹⁸ x (1 + ⅛)] |
| 105 | 327680 | `kNeutral` and `kDenser` | exponential [2¹⁸ x (1 + ¼)] |
| 106 | 360448 | `kDenser` only           | exponential [2¹⁸ x (1 + ⅜)] |
| 107 | 393216 | `kNeutral` and `kDenser` | exponential [2¹⁸ x (1 + ½)] |
| 108 | 425984 | `kDenser` only           | exponential [2¹⁸ x (1 + ⅝)] |
| 109 | 458752 | `kNeutral` and `kDenser` | exponential [2¹⁸ x (1 + ¾)] |
| 110 | 491520 | `kDenser` only           | exponential [2¹⁸ x (1 + ⅞)] |
| 111 | 524288 | `kNeutral` and `kDenser` | exponential [2¹⁹ x (1 + 0)] |
| 112 | 589824 | `kDenser` only           | exponential [2¹⁹ x (1 + ⅛)] |
| 113 | 655360 | `kNeutral` and `kDenser` | exponential [2¹⁹ x (1 + ¼)] |
| 114 | 720896 | `kDenser` only           | exponential [2¹⁹ x (1 + ⅜)] |
| 115 | 786432 | `kNeutral` and `kDenser` | exponential [2¹⁹ x (1 + ½)] |
| 116 | 851968 | `kDenser` only           | exponential [2¹⁹ x (1 + ⅝)] |
| 117 | 917504 | `kNeutral` and `kDenser` | exponential [2¹⁹ x (1 + ¾)] |
| 118 | 983040 | `kNeutral` and `kDenser` | exponential [2¹⁹ x (1 + ⅞)] |

### 16 Bytes Alignment (Typically 64-bit Systems)

| Index | Size | Bucket Distribution | Originating Formula |
| -: | -: | :- | :- |
|   0 |     16 | `kNeutral` and `kDenser` | linear [16 x 1] |
|   1 |     32 | `kNeutral` and `kDenser` | linear [16 x 2] |
|   2 |     48 | `kNeutral` and `kDenser` | linear [16 x 3] |
|   3 |     64 | `kNeutral` and `kDenser` | linear [16 x 4] |
|   4 |     80 | `kNeutral` and `kDenser` | linear [16 x 5] |
|   5 |     96 | `kNeutral` and `kDenser` | linear [16 x 6] |
|   6 |    112 | `kNeutral` and `kDenser` | linear [16 x 7] |
|   7 |    128 | `kNeutral` and `kDenser` | linear [16 x 8] yet exponential [2⁷ x (1 + 0)] |
|   8 |    144 | `kNeutral` and `kDenser` | linear [16 x 9] yet exponential [2⁷ x (1 + ⅛)] |
|   9 |    160 | `kNeutral` and `kDenser` | linear [16 x 10] yet exponential [2⁷ x (1 + ¼)] |
|  10 |    176 | `kNeutral` and `kDenser` | linear [16 x 11] yet exponential [2⁷ x (1 + ⅜)] |
|  11 |    192 | `kNeutral` and `kDenser` | linear [16 x 12] yet exponential [2⁷ x (1 + ½)] |
|  12 |    208 | `kNeutral` and `kDenser` | linear [16 x 13] yet exponential [2⁷ x (1 + ⅝)] |
|  13 |    224 | `kNeutral` and `kDenser` | linear [16 x 14] yet exponential [2⁷ x (1 + ¾)] |
|  14 |    240 | `kNeutral` and `kDenser` | linear [16 x 15] yet exponential [2⁷ x (1 + ⅞)] |
|  15 |    256 | `kNeutral` and `kDenser` | linear [16 x 16] yet exponential [2⁸ x (1 + 0)] |
|  16 |    288 | `kDenser` only           | exponential [2⁸ x (1 + ⅛)] |
|  17 |    320 | `kNeutral` and `kDenser` | exponential [2⁸ x (1 + ¼)] |
|  18 |    352 | `kDenser` only           | exponential [2⁸ x (1 + ⅜)] |
|  19 |    384 | `kNeutral` and `kDenser` | exponential [2⁸ x (1 + ½)] |
|  20 |    416 | `kDenser` only           | exponential [2⁸ x (1 + ⅝)] |
|  21 |    448 | `kNeutral` and `kDenser` | exponential [2⁸ x (1 + ¾)] |
|  22 |    480 | `kDenser` only           | exponential [2⁸ x (1 + ⅞)] |
|  23 |    512 | `kNeutral` and `kDenser` | exponential [2⁹ x (1 + 0)] |
|  24 |    576 | `kDenser` only           | exponential [2⁹ x (1 + ⅛)] |
|  25 |    640 | `kNeutral` and `kDenser` | exponential [2⁹ x (1 + ¼)] |
|  26 |    704 | `kDenser` only           | exponential [2⁹ x (1 + ⅜)] |
|  27 |    768 | `kNeutral` and `kDenser` | exponential [2⁹ x (1 + ½)] |
|  28 |    832 | `kDenser` only           | exponential [2⁹ x (1 + ⅝)] |
|  29 |    896 | `kNeutral` and `kDenser` | exponential [2⁹ x (1 + ¾)] |
|  30 |    960 | `kDenser` only           | exponential [2⁹ x (1 + ⅞)] |
|  31 |   1024 | `kNeutral` and `kDenser` | exponential [2¹⁰ x (1 + 0)] |
|  32 |   1152 | `kDenser` only           | exponential [2¹⁰ x (1 + ⅛)] |
|  33 |   1280 | `kNeutral` and `kDenser` | exponential [2¹⁰ x (1 + ¼)] |
|  34 |   1408 | `kDenser` only           | exponential [2¹⁰ x (1 + ⅜)] |
|  35 |   1536 | `kNeutral` and `kDenser` | exponential [2¹⁰ x (1 + ½)] |
|  36 |   1664 | `kDenser` only           | exponential [2¹⁰ x (1 + ⅝)] |
|  37 |   1792 | `kNeutral` and `kDenser` | exponential [2¹⁰ x (1 + ¾)] |
|  38 |   1920 | `kDenser` only           | exponential [2¹⁰ x (1 + ⅞)] |
|  39 |   2048 | `kNeutral` and `kDenser` | exponential [2¹¹ x (1 + 0)] |
|  40 |   2304 | `kDenser` only           | exponential [2¹¹ x (1 + ⅛)] |
|  41 |   2560 | `kNeutral` and `kDenser` | exponential [2¹¹ x (1 + ¼)] |
|  42 |   2816 | `kDenser` only           | exponential [2¹¹ x (1 + ⅜)] |
|  43 |   3072 | `kNeutral` and `kDenser` | exponential [2¹¹ x (1 + ½)] |
|  44 |   3328 | `kDenser` only           | exponential [2¹¹ x (1 + ⅝)] |
|  45 |   3584 | `kNeutral` and `kDenser` | exponential [2¹¹ x (1 + ¾)] |
|  46 |   3840 | `kDenser` only           | exponential [2¹¹ x (1 + ⅞)] |
|  47 |   4096 | `kNeutral` and `kDenser` | exponential [2¹² x (1 + 0)] |
|  48 |   4608 | `kDenser` only           | exponential [2¹² x (1 + ⅛)] |
|  49 |   5120 | `kNeutral` and `kDenser` | exponential [2¹² x (1 + ¼)] |
|  50 |   5632 | `kDenser` only           | exponential [2¹² x (1 + ⅜)] |
|  51 |   6144 | `kNeutral` and `kDenser` | exponential [2¹² x (1 + ½)] |
|  52 |   6656 | `kDenser` only           | exponential [2¹² x (1 + ⅝)] |
|  53 |   7168 | `kNeutral` and `kDenser` | exponential [2¹² x (1 + ¾)] |
|  54 |   7680 | `kDenser` only           | exponential [2¹² x (1 + ⅞)] |
|  55 |   8192 | `kNeutral` and `kDenser` | exponential [2¹³ x (1 + 0)] |
|  56 |   9216 | `kDenser` only           | exponential [2¹³ x (1 + ⅛)] |
|  57 |  10240 | `kNeutral` and `kDenser` | exponential [2¹³ x (1 + ¼)] |
|  58 |  11264 | `kDenser` only           | exponential [2¹³ x (1 + ⅜)] |
|  59 |  12288 | `kNeutral` and `kDenser` | exponential [2¹³ x (1 + ½)] |
|  60 |  13312 | `kDenser` only           | exponential [2¹³ x (1 + ⅝)] |
|  61 |  14336 | `kNeutral` and `kDenser` | exponential [2¹³ x (1 + ¾)] |
|  62 |  15360 | `kDenser` only           | exponential [2¹³ x (1 + ⅞)] |
|  63 |  16384 | `kNeutral` and `kDenser` | exponential [2¹⁴ x (1 + 0)] |
|  64 |  18432 | `kDenser` only           | exponential [2¹⁴ x (1 + ⅛)] |
|  65 |  20480 | `kNeutral` and `kDenser` | exponential [2¹⁴ x (1 + ¼)] |
|  66 |  22528 | `kDenser` only           | exponential [2¹⁴ x (1 + ⅜)] |
|  67 |  24576 | `kNeutral` and `kDenser` | exponential [2¹⁴ x (1 + ½)] |
|  68 |  26624 | `kDenser` only           | exponential [2¹⁴ x (1 + ⅝)] |
|  69 |  28672 | `kNeutral` and `kDenser` | exponential [2¹⁴ x (1 + ¾)] |
|  70 |  30720 | `kDenser` only           | exponential [2¹⁴ x (1 + ⅞)] |
|  71 |  32768 | `kNeutral` and `kDenser` | exponential [2¹⁵ x (1 + 0)] |
|  72 |  36864 | `kDenser` only           | exponential [2¹⁵ x (1 + ⅛)] |
|  73 |  40960 | `kNeutral` and `kDenser` | exponential [2¹⁵ x (1 + ¼)] |
|  74 |  45056 | `kDenser` only           | exponential [2¹⁵ x (1 + ⅜)] |
|  75 |  49152 | `kNeutral` and `kDenser` | exponential [2¹⁵ x (1 + ½)] |
|  76 |  53248 | `kDenser` only           | exponential [2¹⁵ x (1 + ⅝)] |
|  77 |  57344 | `kNeutral` and `kDenser` | exponential [2¹⁵ x (1 + ¾)] |
|  78 |  61440 | `kDenser` only           | exponential [2¹⁵ x (1 + ⅞)] |
|  79 |  65536 | `kNeutral` and `kDenser` | exponential [2¹⁶ x (1 + 0)] |
|  80 |  73728 | `kDenser` only           | exponential [2¹⁶ x (1 + ⅛)] |
|  81 |  81920 | `kNeutral` and `kDenser` | exponential [2¹⁶ x (1 + ¼)] |
|  82 |  90112 | `kDenser` only           | exponential [2¹⁶ x (1 + ⅜)] |
|  83 |  98304 | `kNeutral` and `kDenser` | exponential [2¹⁶ x (1 + ½)] |
|  84 | 106496 | `kDenser` only           | exponential [2¹⁶ x (1 + ⅝)] |
|  85 | 114688 | `kNeutral` and `kDenser` | exponential [2¹⁶ x (1 + ¾)] |
|  86 | 122880 | `kDenser` only           | exponential [2¹⁶ x (1 + ⅞)] |
|  87 | 131072 | `kNeutral` and `kDenser` | exponential [2¹⁷ x (1 + 0)] |
|  88 | 147456 | `kDenser` only           | exponential [2¹⁷ x (1 + ⅛)] |
|  89 | 163840 | `kNeutral` and `kDenser` | exponential [2¹⁷ x (1 + ¼)] |
|  90 | 180224 | `kDenser` only           | exponential [2¹⁷ x (1 + ⅜)] |
|  91 | 196608 | `kNeutral` and `kDenser` | exponential [2¹⁷ x (1 + ½)] |
|  92 | 212992 | `kDenser` only           | exponential [2¹⁷ x (1 + ⅝)] |
|  93 | 229376 | `kNeutral` and `kDenser` | exponential [2¹⁷ x (1 + ¾)] |
|  94 | 245760 | `kDenser` only           | exponential [2¹⁷ x (1 + ⅞)] |
|  95 | 262144 | `kNeutral` and `kDenser` | exponential [2¹⁸ x (1 + 0)] |
|  96 | 294912 | `kDenser` only           | exponential [2¹⁸ x (1 + ⅛)] |
|  97 | 327680 | `kNeutral` and `kDenser` | exponential [2¹⁸ x (1 + ¼)] |
|  98 | 360448 | `kDenser` only           | exponential [2¹⁸ x (1 + ⅜)] |
|  99 | 393216 | `kNeutral` and `kDenser` | exponential [2¹⁸ x (1 + ½)] |
| 100 | 425984 | `kDenser` only           | exponential [2¹⁸ x (1 + ⅝)] |
| 101 | 458752 | `kNeutral` and `kDenser` | exponential [2¹⁸ x (1 + ¾)] |
| 102 | 491520 | `kDenser` only           | exponential [2¹⁸ x (1 + ⅞)] |
| 103 | 524288 | `kNeutral` and `kDenser` | exponential [2¹⁹ x (1 + 0)] |
| 104 | 589824 | `kDenser` only           | exponential [2¹⁹ x (1 + ⅛)] |
| 105 | 655360 | `kNeutral` and `kDenser` | exponential [2¹⁹ x (1 + ¼)] |
| 106 | 720896 | `kDenser` only           | exponential [2¹⁹ x (1 + ⅜)] |
| 107 | 786432 | `kNeutral` and `kDenser` | exponential [2¹⁹ x (1 + ½)] |
| 108 | 851968 | `kDenser` only           | exponential [2¹⁹ x (1 + ⅝)] |
| 109 | 917504 | `kNeutral` and `kDenser` | exponential [2¹⁹ x (1 + ¾)] |
| 110 | 983040 | `kNeutral` and `kDenser` | exponential [2¹⁹ x (1 + ⅞)] |
