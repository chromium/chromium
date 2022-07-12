# MTECheckedPtr Reference

*** note
**TL;DR** - You don't need to worry about MTECheckedPtr.

For now, this is a matter solely for the Chrome Memory Team to deal
with. The associated TODOs are informational, not actionable. They
will eventually go away.
***

MTECheckedPtr is an alternative implementation of `raw_ptr<T>`.
The canonical implementation of `raw_ptr<T>` is currently BackupRefPtr.
MTECheckedPtr is not enabled by default for anybody, so you should
not be impacted by anything related.

## Description

***aside
This is the brief version. Googlers can search internally for further
reading.
***

MTECheckedPtr is a Chromium-specific implementation of ARM's
[MTE concept][arm-mte]. When MTECheckedPtr is enabled,

*   each `raw_ptr<T>` is assigned a tag,
*   each tag is stored in the top bits of each `raw_ptr<T>`
    and inside PartitionAlloc metadata, and
*   the tags are compared on each dereference.

When an MTECheckedPtr is `free()`d, PartitionAlloc changes the
internally associated tag. Subsequent accesses (e.g. from a
laundered pointer with the old tag value) will not match the new
tag value, at which point PartitionAlloc triggers an immediate crash.

Subjectively, MTECheckedPtr is less forgiving than BackupRefPtr, and
work is ongoing to make CQ pass when enabled. (See the long train of CLs
beginning from https://crbug.com/1298696#c12.)

## Informational TODOs

If you've come to investigate comments like

```
// TODO(crbug.com/1298696): Breaks foo_unittests.
```

or

```
// TODO(crbug.com/1298696): foo_unittests breaks with MTECheckedPtr
// enabled. Triage.
```

This just means that there is a potential UaF in the associated
`raw_ptr<T>`, surfaced by running `foo_unittests`. The Memory Team has
disabled protection (only when `raw_ptr<T>` is MTECheckedPtr, which is
not true unless explicitly enabled) on the same (e.g.
`raw_ptr<T, DegradeToNoOpWhenMTE>`).

When time permits, the Memory Team will look at these issues (but there
are rather a lot of them), triage them, and see what can be fixed.

In the meantime, code OWNERS need only note that these comments are
*informational* and *not actionable*. They have a similar standing
as `DanglingUntriaged` (see
[dangling_ptr.md](../../docs/dangling_ptr.md)) and co-opt the same
template parameter.

## Appendix: Degradation Template Parameters

As described in `raw_ptr.h`, we use the following `D`s when given
a particular `raw_ptr<T, D>`.

When MTECheckedPtr is not enabled (the default):

*   `DegradeToNoOpWhenMTE` - No effect. Uses the default `raw_ptr<T>`
    implementation (currently BackupRefPtr).
*   `DanglingUntriagedDegradeToNoOpWhenMTE` - No effect. Uses the
    default `raw_ptr<T>` implementation (currently BackupRefPtr),
    but permits dangling pointers.

When MTECheckedPtr *is* enabled (not the default for anybody),
both of the above degrade the `raw_ptr<T, D>` into the no-op version
of `raw_ptr`.

[arm-mte]: https://community.arm.com/arm-community-blogs/b/architectures-and-processors-blog/posts/enhancing-memory-safety
