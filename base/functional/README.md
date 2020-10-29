# base/functional library

[TOC]

## What goes here

This directory contains function objects from future STL versions and closely
related types.

Things should be moved here that are generally applicable across the code base.
Don't add things here just because you need them in one place and think others
may someday want something similar. You can put specialized function objects in
your component's directory and we can promote them here later if we feel there
is broad applicability.

### Design and naming

Fundamental [//base principles](../README.md#design-and-naming) apply, i.e.:

Function objects should either come directly from the STL or adhere as closely
to STL as possible. Functions and behaviors not present in STL should only be
added when they are related to the specific function objects.

For STL-like function objects our policy is that they should use STL-like naming
even when it may conflict with the style guide. So functions and class names
should be lower case with underscores. Non-STL-like classes and functions should
use Google naming. Be sure to use the base namespace.
