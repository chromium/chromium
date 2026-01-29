---
applyTo: "**/*.cc,**/*.h"
---

# C++ Development Guidelines

The C++ style guide documents are located [here](../../styleguide/c++/). Please
read and consider them while editing C++ source files.

- Always follow the [Chromium C++ style guide](../../styleguide/c++/c++.md).
  - Note that the Chromium style guide inherits from the larger
    [Google C++ Style Guide](https://google.github.io/styleguide/cppguide.html),
    which should also be followed.
- Consider the [C++ Dos and Don'ts](../../styleguide/c++/c++-dos-and-donts.md)
  as a set of "best practices" and apply them when convenient.
- When dealing with const and constexpr, consider
  [Using Const Correctly](../../styleguide/c++/const.md) and
  [Defining compile-time constants correctly](../../styleguide/c++/defining_compile_time_const.md).
- When dealing with `CHECK`s, logging, and other failure codepaths, consider
  [CHECK(), DCHECK() and NOTREACHED()](../../styleguide/c++/checks.md).
- Only use C++ language and library features allowed by
  [Modern C++ use in Chromium](../../styleguide/c++/c++-features.md).
- The [Blink C++ Style Guide](../../styleguide/c++/blink-c++.md) should only be
  applied when editing code inside the
  [Blink directory](../../third_party/blink/).
