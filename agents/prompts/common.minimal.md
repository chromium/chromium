# Gemini-CLI Specific Directives

Instructions that apply only to gemini-cli.

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

* Do not attempt a build or compile without first establishing the correct
  output directory. If you have not been told the directory, ask for it.
* Unless otherwise instructed, build with: `autoninja --quiet -C {OUT_DIR} {TARGET}`

## Testing

Unless otherwise instructed, run tests with:
`tools/autotest.py --quiet --run-all -C {OUT_DIR} {RELEVANT_TEST_FILENAMES}`
