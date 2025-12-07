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
[ ] 1. Review user input
[ ] 2. Find the right `BUILD.gn` file
[ ] 3. Consider updating the `BUILD.gn` file
[ ] 4. Wait for the user to confirm they have updated the `BUILD.gn` file
[ ] 5. Rename Test File
[ ] 6. Tell the user to format, stage and commit the changes
[ ] 7. Tell the user to proceed to refactoring part 2
```

## Review user input
Review the following information before messaging the user so you can help them
effectively.

You are responsible for determining the following variables:
  - `${out_dir}`: The build directory (e.g., `out/debug_x64`).

- The user may launch this prompt with syntax
  such as `out/debug_x64`, if they do you should parse the input into the above
  variables.
- The user may have specified `## Developer Prompt Variables`. If they have,
  you should that as the `${out_dir}` unless the user respecified it above.

### If the user did not provide satisfactory input
- If the user did not provide input, or provided some input, but did not provide
satisfactory input, to know `${out_dir}` . You can let them know that they can
add the following code block to their
[copilot-instructions.md](../copilot-instructions.md) file to set the default
`${out_dir}`.
  ```markdown
  ## Developer Prompt Variables
  `${out_dir}` = `debug_x64`
  ```

## Find the right `BUILD.gn` file
You will need to find the file that references the test file you are
refactoring. You can find this with the following command:
`gn refs out/{out_dir} ${file}`

- If the output is `//chrome/browser/foo:bar`, the file will be
  `chrome/browser/foo/BUILD.gn` and inside of the `${target}` `bar`.
   You do not need to search for the `BUILD.gn` file, you can open it directly
   since chrome is accessible relative to this file at
   [`../../chrome/](../../chrome/).


## Rename Test File
You **must** use `git mv` to rename the test file `${file}`.
- The new filename should replace the `_unittest.cc` suffix with
  `_browsertest.cc`.
- *Example*: If `${file}` is `foo_unittest.cc`, the command would be
  `git mv foo_unittest.cc foo_browsertest.cc`.

## Consider updating the `BUILD.gn` file
Check the file length of the `BUILD.gn` file you found in the previous step.
- You must do this using `(Get-Content ${file}).Count`

## If the file is under 4000 lines
You should attempt to follow the instructions below to update the
`BUILD.gn` file yourself.

## If the file is over 4000 lines
If the file is over 4000 lines, tell the user the file is too long for your
context window. Tell the user they need to manually update the `BUILD.gn` file
since its too long for your context window.

### Updating the `BUILD.gn` file
When refactoring a unit test to a browser test in Chromium, you will need to
update the `BUILD.gn` file. This is a critical step because the build system
needs to know that your test file has moved from the unit tests target to the
browser tests target.

You will need to explain your chain of thought and follow these step by step:
```markdown
[ ] 1. Find your original test file in the unit_tests section
[ ] 2. Remove the entry from unit_tests
[ ] 3. Add to browser_tests
[ ] 4. Review modified lines
```

#### Find your original test file in the `${target}` section
   - Look for your original file path in the the `${target}`, this will likely
     be a `test("unit_tests")` or `source_set("...unit_tests")` section
   - The file path will be formatted as a relative path, like:
     `"../browser/path/to/your_unittest.cc"`
   - For example, if your original file was at
     `chrome/browser/ui/views/my_feature_unittest.cc`,
     you'd look for `"../browser/ui/views/my_feature_unittest.cc"` in the
     sources list
   - you can tell them the name of the file they are looking for

#### Remove the file entry from unit_tests
   - Delete the file entry only line from the `sources = [ ... ]` list in the
     `${target}` section
  - Do not delete the entire `sources = [ ... ]` block, just the line with your
    original test file
  - For example, if you are removing the line:
    `../browser/ui/views/my_feature_unittest_2.cc`,
    ```gn
    # Before
    # sources = [
    #   ...,
    #   "../browser/ui/views/my_feature_unittest_1.cc",
    #   "../browser/ui/views/my_feature_unittest_2.cc",
    #   "../browser/ui/views/my_feature_unittest_3.cc",
    #   ...,
    # ]
    # After
    sources = [
      ...,
      "../browser/ui/views/my_feature_unittest_1.cc",
      "../browser/ui/views/my_feature_unittest_3.cc",
      ...,
    ]
    ```

#### Find the location to add the new browser test
   - Look for the `browser_tests` section in the same `BUILD.gn` file
   - This will likely be a `test("browser_tests")` or
     `source_set("...browser_tests")` section
   - If you don't see a `browser_tests` section, you may need to create one and
     hook it up to `chrome/test/BUILD.gn`.
   - Pay attention to any `if` statement blocks that might be surrounding your
     test file
   - If your original file was inside a conditional block like `if (is_win)` or
     `if (enable_extensions)`, make sure to add your new browsertest file in the
     corresponding conditional block in the browser_tests section

#### Add to browser_tests
   - You must **add** a new line with your renamed file path to its
     `sources = [ ... ]` list
   - Keep the same relative path format, just change the filename suffix
   - For example, if you removed `"../browser/ui/views/my_feature_unittest.cc"`,
     you'd add `"../browser/ui/views/my_feature_browsertest.cc"`

#### Review modified lines
   - After making these changes, review the modified lines in the `BUILD.gn`
     file to ensure they are correct
   - Make sure you have removed the old unit test file and added the new
     browser test file correctly
   - No other lines should be modified

Before moving on you must **wait** for the user to confirm that the `BUILD.gn`
file has been code reviewed by the user and updated correctly.

## Tell the user to format, stage and commit the changes
After `BUILD.gn` file has been updated you **must** run the following commands
in the following step by step order:
```markdown
[ ] 1. `git cl format ${file} [build.gn file]`
[ ] 2. `git add ${file} [build.gn file]`
[ ] 3. `git commit -m "[Bedrock]: Refactor ${file} _unittest to _browsertest"`
[ ] 4. `git cl upload`
```

Tell the user they must run `git cl upload` before continuing to part 2
to upload the changes the code review system before continuing the refactor so
that code reviewers can clearly disambiguate the changes made in this part of
the refactor compared to the base test. There is a bug in gerrit that causes
large diffs of moved files to not show up correctly, so we want to
upload the changes in two parts to work around
[this known issue](https://g-issues.gerritcodereview.com/issues/40003516).

## Tell the user to proceed to refactoring part 2
After completing the above steps, provide the user with a clickable link to
their new file and tell the user to start a new chat and run
[/refactor_bedrock_part_2](refactor_bedrock_test_part_2.prompt.md)
with the new file path.
