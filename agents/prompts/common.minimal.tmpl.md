# Gemini-CLI Specific Directives

Instructions that apply only to gemini-cli.

<!-- E.g. tools/metrics/histograms/enums.xml -->
* When using the `read_file` tool:
  * Always set the 'limit' parameter to 20000 to prevent truncation.
* File Not Found Errors:
  * If a file operation fails due to an incorrect path, do not retry with the
    same path.
  * Inform the user and search for the correct path using parts of the path or
    filename.

# Common Directives

Instructions that are useful for chromium development, and not specific to a
single agentic tool.

## Paths

* All files in chromium’s source can be read by substituting `chromium/src` or
  `//` for the current workspace (which can be determined by running `gclient
  root` and appending `/src` to the output).

## Building

<!-- It's possible to try and get the agent to detect the output directory, but
the times it guesses wrong are sufficiently disruptive that it's better to just
ensure the directory has been specified. -->
* Do not attempt a build without first establishing the correct output
  directory and target. If you have not been given them, and you plan on doing
  a build, then stop and ask before starting on any other tasks.
<!-- Not that important since autoninja.py automatically adds --quiet when
GEMINI_CLI=1 is set, but nice for the command to be correct from the start.-->
* Unless otherwise instructed, build with: `autoninja --quiet -C {OUT_DIR} {TARGET}`
  * If given an `autoninja` command that is missing `--quiet`, add `--quiet`.

## Testing

Unless otherwise instructed, run tests with:
`tools/autotest.py --quiet --run-all -C {OUT_DIR} {RELEVANT_TEST_FILENAMES}`

When using `tools/autotest.py`:
<!-- Avoid passing nonexistent/irrelevant build targets to `autoninja`. -->
* Do not invoke `autoninja` beforehand because `autotest.py` automatically
  builds relevant targets.
* Build targets containing colons (`:`) are not valid inputs for
  `{RELEVANT_TEST_FILENAMES}`.

## Coding

<!-- Try to prevent agent from fixing TODOs that appear close to code that it's
rewriting. -->
* Stay on task: Do not address code health issues or TODOs in code unless it is
  required to achieve your given task.
<!-- Try to prevent useless comments that state the obvious. -->
* Add code comments sparingly: Focus on *why* something is done, not *what* is
  done.

## Presubmit Checks

When you have finished validating your changes through other means, run:

```sh
git cl format
git cl presubmit -u --force
```
* Fix errors / warnings related to your change, but do not fix pre-existing
  warnings (from lines that you did not change).

## JNI

<!--
We have a separate //agents/prompts/templates/android.md, but the purpose
of that file is to tell the agent to build / test on Android. There are a lot
of shared source files that contain JNI, so including this short explanation
improves outcomes even when Android-specific changes are not the focus.
-->
Chrome on Android uses both Java and C++ frequently. Our JNI uses codegen from
`//third_party/jni_zero`.

Identifying JNI methods:
  * In Java, methods annotated with `@CalledByNative` are called by C++.
    * In C++, these appear as methods with a "Java_" prefix.
  * In Java, methods within interfaces annotated with `@NativeMethods` are calls
    into C++.
    * In C++, these appear as methods with a "JNI_" prefix.
    * If the first parameter is of type `long` and has a prefix of "native",
      then it maps to a method of the same name on the C++ class that comes
      after the "native" prefix. Eg. `void foo(long nativeBarImpl)` means you
      need to find `BarImpl::Foo` in C++.

Finding JNI methods:
  * To find the Java side of JNI: search for the file named `{JavaClass}.java`
    (where {JavaClass} is taken from the `#include "{JavaClass}_jni.h"`.
  * To find the C++ side of JNI: Search for the text `{JavaClass}_jni.h` in C++
    files (where {JavaClass} is taken from the .java file name).

When making changes to JNI methods, always make changes to both the .java and
.cc/.h files involved.
