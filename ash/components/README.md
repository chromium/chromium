DEPRECATED. If you're about adding new component for ash-chrome only,
consider using //chromeos/ash/components.

# About //ash/components

This directory contains components that are used by //ash system UI and window
manager code. It sits "below" //ash in the dependency graph. For C++ code,
think of //ash/components like top-level //components, but for code that is
only used on ChromeOS, and only for system UI / window manager support.

Much of this code used to live in //chromeos/components. The
[Lacros project](/docs/lacros.md) is extracting browser functionality into a
separate binary. As part of this migration, code used only by the ash-chrome
system UI binary moved into "ash" directories. See the
[Chrome OS source directory migration](https://docs.google.com/document/d/1g-98HpzA8XcoGBWUv1gQNr4rbnD5yfvbtYZyPDDbkaE/edit)
design doc for details.
