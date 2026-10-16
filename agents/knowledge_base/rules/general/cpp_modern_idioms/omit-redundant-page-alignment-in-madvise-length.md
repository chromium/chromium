---
name: general/omit-redundant-page-alignment-in-madvise-length
rubric: readability
severity: optional
---

**Rule.** When calling `madvise()`, pass the computed byte length directly
without manually rounding or aligning it up to a page boundary (such as via
`bits::AlignUp` or `base::GetPageSize()`). While the starting address passed to
`madvise()` must be page-aligned, the kernel automatically rounds the `length`
argument up to a multiple of the system page size.

**Why.** Per the POSIX and Linux madvise(2) specifications, the kernel
automatically rounds the length parameter up to the page size. Manually rounding
up length in user space adds redundant arithmetic boilerplate and unnecessary
complexity.

## Examples

Bad:

```cpp
// Redundant manual page alignment of length.
size_t length = bits::AlignUp(end - current, base::GetPageSize());
if (madvise(reinterpret_cast<void*>(current), length, MADV_WILLNEED) != 0) {
  PLOG(WARNING) << "madvise failed";
}
```

Good:

```cpp
// The kernel automatically rounds `length` up to the system page size.
size_t length = end - current;
if (madvise(reinterpret_cast<void*>(current), length, MADV_WILLNEED) != 0) {
  PLOG(WARNING) << "madvise failed";
}
```
