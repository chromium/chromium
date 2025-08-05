# Prompt Evaluation

This directory contains an experimental script for running prompt evaluation
tests on extensions and prompts under `//agents`. It currently only works
locally and will make temporary changes to your Chromium repo.

## Usage

Existing tests can be run via the `//agents/testing/eval_prompts.py` script. It
should handle everything automatically, although it is advised to commit any
changes before running this script. It will automatically retrieve a temporary
copy of promptfoo, perform repo setup, run configured tests, and perform
teardown.

By default, it will build promptfoo from ToT, but specific behavior can be
configured via command line arguments, including use of stable releases via npm
which will likely result in faster setup.

## Adding Extensions

The script only installs the extensions in the `EXTENSIONS_TO_INSTALL` list at
the top of the file. If an extension should be present for testing, add the
extension name to this list.

## Adding Tests

Each independent test case should have its own promptfoo yaml config file. See
the [promptfoo documentation](https://www.promptfoo.dev/docs/configuration/guide/)
for more information on this. If multiple prompts are expected to result in the
same behavior, and thus can be tested in the same way, the config file can
contain multiple prompts. promptfoo will automatically test each prompt
individually.

Config files should be placed in a `tests/promptfoo/` subdirectory of the
relevant prompt or extension directory. After they exist on disk, new yaml
files will need to be added to the `PROMPTFOO_CONFIG_COMPONENTS` list at the
top of the script for the tests to actually be run.
