# Tasks Directory

This directory contains prompts for various software engineering tasks that can
be executed by an agent. Each subdirectory represents a single, self-contained
task.

In addition to being a reference for new tasks, these are intended to be used as
an eval set for regression testing on a Chromium CI builder.

## Subdirectory Structure

Each subdirectory should contain all the necessary files and information for the
agent to perform the task. This includes:

- `prompt.md`: The prompt that initiates the task.
- `README.md`: A file describing the task and its outcome.

## README.md Format

**Note**: this format is subject to change as a regression test suite is
implemented.

The `README.md` file in each subdirectory should follow this format:

- **Owner**: List or person to contact if the task stops reproducing.
- **Description**: A brief description of the task.
- **Git-Revision**: The git revision on which the task was successfully
  performed. This is used for reproducibility.
- **Result**: A summary of what the agent accomplished.
- **Modified files**: A list of the files that were modified by the agent during
  the task.

This structure ensures that each task is well-documented and can be easily
understood and reproduced.
