# `partition_alloc_base/`

This is a rough mirror of Chromium's `//base`, cut down to the necessary
files and contents that PartitionAlloc pulls in. Small tweaks (n.b.
macro renaming) have been made to prevent compilation issues, but we
generally prefer that this be a mostly unmutated subset of `//base`.

## Update Policy

TBD.

*   This directory may drift out of sync with `//base`.

*   We will merge security updates from Chromium's `//base` once we are
    made aware of them.

*   We may elect to freshen files when we need to use new `//base`
    functionality in PA.

## Augmentation Policy

Prefer not to directly modify contents here. Add them into
`augmentations/`, documenting the usage and provenance of each addition.
