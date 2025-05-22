---
mode: "agent"
description: "Rename a C++ unit test file to an In-Process Browser Test in Chromium."
---
# Chromium Code Refactoring: Rename Unit Test to Browser Test

You are an AI assistant with 10 years of experience writing Chromium unit tests
and browser tests. Your task is to start refactoring the C++ test file `${file}`
from a unit test to an In-Process Browser Test by renaming the file and
updating the corresponding `BUILD.gn` file.

## Step by step instructions

```markdown
[ ] 0. Before you start
[ ] 1. Rename Test File
[ ] 2. Tell the user they must manually update `chrome/test/BUILD.gn`
[ ] 3. Tell the user they must format, stage and commit the changes
[ ] 4. Tell the user to proceed to refactoring part 2
```

## Rename Test File
You **must** use `git mv` to rename the test file `${file}`.
- The new filename should replace the `_unittest.cc` suffix with
  `_browsertest.cc`.
- *Example*: If `${file}` is `foo_unittest.cc`, the command would be
  `git mv foo_unittest.cc foo_browsertest.cc`.

## Instructions for Updating `chrome/test/BUILD.gn`
Tell the user they need to manually update the `chrome/test/BUILD.gn` file since
its too long for your context window.

When refactoring a unit test to a browser test in Chromium, they will need to
manually update the `chrome/test/BUILD.gn` file. This is a critical step because
the build system needs to know that your test file has moved from the unit tests
target to the browser tests target.

Here's what they will need to do:

1. **Find your original test file in the unit_tests section**:
   - Look for your original file path in the `test("unit_tests")` section
   - The file path will be formatted as a relative path, like:
     `"../browser/path/to/your_unittest.cc"`
   - For example, if your original file was at
     `chrome/browser/ui/views/my_feature_unittest.cc`,
     you'd look for `"../browser/ui/views/my_feature_unittest.cc"` in the
     sources list
   - you can tell them the name of the file they are looking for

2. **Remove the entry from unit_tests**:
   - Delete this line completely from the `sources = [ ... ]` list in the
     `test("unit_tests")` section

3. **Add to browser_tests**:
   - Find the `test("browser_tests")` section in the file
   - Add a new line with your renamed file path to its `sources = [ ... ]` list
   - Keep the same relative path format, just change the filename suffix
   - For example, if you removed `"../browser/ui/views/my_feature_unittest.cc"`,
     you'd add `"../browser/ui/views/my_feature_browsertest.cc"`
   - Let them know you will run a format tool to ensure the file is formatted
     and sorted correctly

4. **Respect any conditional blocks**:
   - Pay attention to any `if` statement blocks that might be surrounding your
     test file
   - If your original file was inside a conditional block like `if (is_win)` or
     `if (enable_extensions)`, make sure to add your new browsertest file in the
     corresponding conditional block in the browser_tests section

Wait for the user to confirm that they have updated the `chrome/test/BUILD.gn`
file.

## Tell the user to format, stage and commit the changes
After the user has updated the `chrome/test/BUILD.gn` file, you **must** run the
following commands:

1. `gn add ${file} chrome/test/BUILD.gn`
2. `git cl format`
3. `gn add ${file} chrome/test/BUILD.gn`
4. `git commit -m "[Bedrock]: Refactor ${file} _unittest to _browsertest"`

## Tell the user to proceed to refactoring part 2
After completing the above steps, provide the user with a clickable link to
their new file and tell the user to start a new chat and run
[/refactor_unit_test_to_browser_test_part_2](refactor_unit_test_to_browser_test_part_2.prompt.md)
with the new file path.
