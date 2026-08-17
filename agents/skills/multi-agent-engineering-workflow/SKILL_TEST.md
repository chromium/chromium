# Multi-Agent Engineering Workflow Test Protocol (SKILL_TEST.md)

This document describes the protocol used to validate the Multi-Agent
Engineering Workflow sub-agents and prevent regressions in the protocol
execution. It defines "unit" tests for each stage of the workflow protocol.

## Objective

To verify that sub-agents adhere to their mandates, follow the TONE MANDATE,
produce valid schema-compliant output, and generate buildable code.

## Methodology: Unit Testing by Stage

Rather than testing the entire protocol end-to-end (which is slow and prone to
flakiness), we test each stage independently by providing mock inputs and
verifying specific expected outputs.

### Test Data Isolation

To allow for real builds without risking side effects on the actual codebase,
all tests operate on **static, checked-in dummy files** located in:
`tests/testdata/` (relative to the skill's root directory)

This directory contains its own `BUILD.gn` file to allow running real builds on
the test outputs!

#### Rules for Flawed Test Files

To prevent automated tooling (like static analyzers) from flagging intentional
flaws in test data, and to prevent humans from trying to fix them:

1. **Extension:** Files containing intentional flaws MUST use the extension
   `.workflow.test` (e.g., `complex_uaf.cc.workflow.test`).
2. **Descriptive Pre-Copy Naming:** The source file in the repository SHOULD
   have a name that indicates the flaw (e.g., `complex_uaf.cc.workflow.test`) to
   make it clear to humans what is being tested.
3. **Realistic Copied Naming:** The build system MUST rename the file to a
   normal, realistic name when copying it and stripping comments (e.g.,
   `bind_post_task_helper.cc`) to prevent the agent from anchoring on the
   filename.
4. **Realistic Code:** Class names, function names, and non-workflow comments in
   the file MUST be structured as if the code was valid and expected to work.
5. **Annotations:** Annotate the files with `// WORKFLOW: <comment>` to describe
   the flaw or reproduction steps for humans.
6. **Preprocessing:** The `BUILD.gn` file MUST contain a GN `action` to copy
   these files, rename them, and strip out the `// WORKFLOW:` comments.
7. **Test Only:** All test targets in `BUILD.gn` MUST be marked
   `testonly = true`.
8. **Line Numbers:** Line numbers in test cases are OPTIONAL and should
   generally be omitted to avoid brittleness caused by line shifts when comments
   are stripped. The primary verification should be based on file paths and
   content patterns rather than exact line numbers.

______________________________________________________________________

## Test Cases Structure

Test cases are defined in `workflow_stage_[name]_tests.json` conforming to
`workflow_test_schemas.json`. Each test case includes:

1. **Stage:** The workflow stage being tested.
2. **Name:** Descriptive name of the test.
3. **Inputs:** Mock files or state provided to the agent.
4. **Expected Outputs:**
   - `files_created`: List of files that should be created.
   - `content_patterns`: Regex patterns that must match file content.
   - `buildable`: Boolean indicating if the output must compile.
   - `valid_json`: Boolean indicating if the output must be valid JSON.

______________________________________________________________________

## How to Run Tests

Run a specific test file from the skill's root directory:
`python3 run_workflow_tests.py --tests tests/workflow_stage_generate_tests.json`

Run all tests using a shell loop from the skill's root directory:
`for f in tests/workflow_stage_*_tests.json; do \`
`  python3 run_workflow_tests.py --tests "$f"; done`

## How to Run Presubmit Checks

To run the presubmit checks locally without uploading to Gerrit:
`python3 run_presubmit.py`

## Manual Test Execution via Agent

If the automated test runner fails or hangs (e.g., due to environment issues
with `agentapi`), an agent can manually execute the tests by interpreting the
JSON files:

1. **Read the Test JSON**: Locate the test case you want to run.
2. **Setup**: Create an isolated directory under the configured `temp_directory`
   (e.g. `agents/skills/multi-agent-engineering-workflow/.temp/`) and copy
   `testdata` into it.
3. **Extract Prompt and Inputs**: Construct a clear prompt for a subagent,
   explaining the role (e.g., Supervisor) and providing the `base_inputs` and
   `override_inputs` from the test case. Instruct the agent to work in the
   isolated directory.
4. **Invoke Subagent**: Use the `invoke_subagent` tool with the constructed
   prompt.
5. **Verify Output**: When the subagent responds, verify that its output matches
   the `expected_outputs` in the test case (e.g., valid JSON, specific content
   patterns).
6. **Cleanup**: Delete the temporary directory after verification.

This ensures that tests can still be run even if the automation script is not
fully functional in the current environment.

## See Also

- [SKILL_TEST_PLAN.md](SKILL_TEST_PLAN.md) for the overall testing strategy and
  methodology.
