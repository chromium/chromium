# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Applying Gemini CLI to Fix Chromium Unsafe Buffer Usage

This is a script to discover and generate spanification fixes for a given file.
It implements a 3-entity workflow:
1. Generator (Entity 1): Generates the patch.
2. Deterministic Checker (Entity 2): Verifies compilation on all 5 platforms.
3. Reviewer (Entity 3): Reviews the patch for safety and idioms.
"""

import argparse
import contextlib
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile
import threading

GEMINI_MD_PATH = 'GEMINI.md'  # Assuming the script is run from src
SCRIPT_DIR = os.path.dirname(__file__)

# Prompts:
FIX_PROMPT_MD = os.path.join(SCRIPT_DIR, 'prompt.md')
REVIEWER_PROMPT_MD = os.path.join(SCRIPT_DIR, 'reviewer_prompt.md')
SPANIFICATION_GEMINI_MD = 'SPANIFICATION_GEMINI_MD'

# `gemini-cli` expected outputs:
GEMINI_OUT_DIR = 'gemini_out'
COMMIT_MESSAGE_PATH = GEMINI_OUT_DIR + '/commit_message.md'
SUMMARY_PATH = GEMINI_OUT_DIR + '/summary.json'

# Make sure patches are built and tests on all platforms.
REQUIRED_BUILD_DIRS = [
    'linux-rel',
    'mac-rel',
    'linux-win-cross-rel',
    'android-14-x64-rel',
    'linux-chromeos-rel',
]


def ensure_gn_build_dir():
    """Ensure that the required GN build directories exist and have correct
    args."""
    base_args = [
        'dcheck_always_on = true',
        'is_component_build = false',
        'is_debug = false',
        'symbol_level = 0',
        'use_remoteexec = true',
        'use_siso = true',
    ]

    configs = {
        'linux-rel': ['target_os = "linux"'],
        'mac-rel': ['target_os = "mac"'],
        'linux-win-cross-rel': ['target_os = "win"'],
        'android-14-x64-rel': ['target_os = "android"'],
        'linux-chromeos-rel': ['target_os = "chromeos"'],
    }

    for build_dir, extra_args in configs.items():
        dir_path = os.path.join('out', build_dir)
        args_gn_path = os.path.join(dir_path, 'args.gn')

        print(f"Ensuring GN build directory '{dir_path}'...")
        if not os.path.exists(dir_path):
            os.makedirs(dir_path)

        # Always write/overwrite args.gn to ensure it's correct
        with open(args_gn_path, 'w', encoding='utf-8') as f:
            f.write('\n'.join(base_args + extra_args) + '\n')

        # Always run 'gn gen' to ensure the ninja files reflect the args.gn.
        print(f"Running 'gn gen {dir_path}'...")
        subprocess.run(['gn', 'gen', dir_path], check=True)


def ensure_docs():
    # Copy ../../../../docs/unsafe_buffers.md to the script directory.
    src_docs_path = os.path.abspath(
        os.path.join(SCRIPT_DIR, '../../../../docs/unsafe_buffers.md'))
    dest_docs_path = os.path.join(SCRIPT_DIR, 'unsafe_buffers.md')
    if not os.path.exists(dest_docs_path):
        shutil.copy2(src_docs_path, dest_docs_path)


@contextlib.contextmanager
def setup_gemini_context_md(context_files):
    """Context manager to temporarily modify GEMINI.md to include the given
    context files (prompts or code files)."""

    def modify_gemini_md(action, new_entry=""):
        content = ""
        if os.path.exists(GEMINI_MD_PATH):
            with open(GEMINI_MD_PATH, 'r', encoding='utf-8') as f:
                content = f.read()
        else:
            print("Error: the script is expected to be run from the src/ "
                  "directory where GEMINI.md is located.")
            sys.exit(1)

        # Use regex to remove the block between the start and end markers.
        # re.DOTALL allows '.' to match newlines.
        pattern = re.compile(
            f"# {SPANIFICATION_GEMINI_MD}.*?"
            f"# /{SPANIFICATION_GEMINI_MD}\n", re.DOTALL)
        cleaned_content = pattern.sub("", content)

        final_content = cleaned_content
        if action == 'add':
            final_content += new_entry

        with open(GEMINI_MD_PATH, 'w', encoding='utf-8') as f:
            f.write(final_content)

    entry = f"# {SPANIFICATION_GEMINI_MD}\n"
    for f in context_files:
        if f:
            entry += f"@{f}\n"
    entry += f"# /{SPANIFICATION_GEMINI_MD}\n"
    modify_gemini_md('add', entry)
    try:
        yield
    finally:
        modify_gemini_md('remove')


def stream_reader(process, output):
    """
    Reads from process.stdout line by line and populates the list.
    This blocks *only* this thread, not the main thread.
    """
    try:
        for line in iter(process.stdout.readline, ''):
            if not line.strip():  # Skip empty lines
                continue
            try:
                json_obj = json.loads(line)
                output.append(json_obj)
                # Print the full JSON object for monitoring tool calls and
                # progress.
                print(json.dumps(json_obj, indent=2))
            except json.JSONDecodeError:
                # This will also print stderr lines since we merged them
                print(line, end='')
    except (IOError, ValueError):
        # This can happen if the pipe is closed abruptly by process.kill()
        pass


def run_gemini(prompt, clear_out_dir=True):
    """
    Run the gemini CLI.
    """
    if clear_out_dir:
        # Delete `gemini_out` directory if it exists, and recreate it.
        # This is where gemini was instructed to write its outputs.
        if os.path.exists(GEMINI_OUT_DIR):
            subprocess.run(['rm', '-rf', GEMINI_OUT_DIR], check=True)
        os.makedirs(GEMINI_OUT_DIR, exist_ok=True)

    cmd = ['gemini']

    # Headless mode. This allows visualizing the tools used and their outputs
    # while running it.
    cmd.extend(['--output-format', 'stream-json'])

    cmd.extend(['--approval-mode', 'auto_edit'])

    policy_file = os.path.join(SCRIPT_DIR, 'policy.toml')
    cmd.extend(['--policy', policy_file])

    # The `--allowed-tools` is deprecated in favor of the policy file, but the
    # policy engine isn't the only source of truth for allowed tools due to
    # a bug in headless mode. So we need to specify both until the bug
    # is fixed. See https://github.com/google-gemini/gemini-cli/issues/20058
    ALLOWED_TOOLS = [
        "read_file", "replace", "write_file", "run_shell_command",
        "remote_code_search", "run_debugging_agent"
    ]
    cmd.extend(['--allowed-tools', ','.join(ALLOWED_TOOLS)])

    output = []
    process = None
    try:
        with subprocess.Popen(
                cmd,
                stdin=subprocess.PIPE,
                stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT,  # Merge stdout and stderr
                text=True,
                encoding='utf-8',
                bufsize=1) as process:
            # stdout
            reader_thread = threading.Thread(target=stream_reader,
                                             args=(process, output))
            reader_thread.daemon = True
            reader_thread.start()

            # stdin
            process.stdin.write(prompt)
            process.stdin.close()

            # Wait for the process to complete or timeout
            exit_code = process.wait(timeout=2700)  # 45 minutes
    except subprocess.TimeoutExpired:
        print("Error: Process timed out after 2700 seconds.", file=sys.stderr)
        if process:
            process.kill()
            process.wait()  # Ensure it's cleaned up
        exit_code = 124  # Standard timeout exit code
    except Exception as e:
        print(f"An error occurred: {e}", file=sys.stderr)
        if process and process.poll() is None:
            process.kill()
            process.wait()
        exit_code = 1  # General error
    finally:
        if 'reader_thread' in locals() and reader_thread.is_alive():
            reader_thread.join(timeout=5.0)

    return exit_code, output


def run_deterministic_check(file_path):
    """Entity 2: Check compilation and run tests."""
    errors = []
    # 1. Compilation
    for build_dir in REQUIRED_BUILD_DIRS:
        print(f"Checking compilation for {build_dir}...")
        cmd = ['autoninja', '-C', f'out/{build_dir}', '--quiet']
        result = subprocess.run(cmd,
                                capture_output=True,
                                text=True,
                                check=False)
        if result.returncode != 0:
            print(f"Compilation failed for {build_dir}")
            return [
                f"Build directory: out/{build_dir}\n"
                f"Stdout:\n{result.stdout}\nStderr:\n{result.stderr}"
            ]
        else:
            print(f"Compilation success for {build_dir}")

    # 2. Testing (Run tests if it's a test file or has corresponding tests)
    # This simulates a real CQ check.
    if not errors:
        print(f"Running tests for {file_path}...")
        # Use linux-rel for testing as a representative platform.
        cmd = [
            './tools/autotest.py', '--quiet', '--run-all', '-C',
            'out/linux-rel', file_path
        ]
        result = subprocess.run(cmd,
                                capture_output=True,
                                text=True,
                                check=False)
        if result.returncode != 0:
            if ("doesn't look like a test file" in result.stderr
                    or "doesn't look like a test file" in result.stdout):
                print(f"No tests found for {file_path}.")
            else:
                print(f"Tests failed for {file_path}")
                errors.append(
                    f"Tests failed:\n{result.stdout}\n{result.stderr}")
        else:
            print("Tests passed!")

    return errors


def run_reviewer(file_path):
    """Entity 3: Run reviewer agent on the specific file changes."""
    print(f"Running Reviewer Agent on {file_path}...")
    # Get the diff for the specific file since we started (HEAD)
    diff_result = subprocess.run(['git', 'diff', 'HEAD', '--', file_path],
                                 capture_output=True,
                                 text=True,
                                 check=False)
    patch = diff_result.stdout
    if not patch:
        return "SUCCESS", "No changes to review."

    with open(REVIEWER_PROMPT_MD, 'r', encoding='utf-8') as f:
        reviewer_prompt_tmpl = f.read()

    # Create a temporary reviewer prompt with the actual patch.
    with tempfile.NamedTemporaryFile(mode='w',
                                     suffix='.md',
                                     delete=False,
                                     encoding='utf-8') as temp_f:
        temp_f.write(reviewer_prompt_tmpl.replace('{{patch}}', patch))
        temp_reviewer_prompt_path = temp_f.name

    try:
        with setup_gemini_context_md([temp_reviewer_prompt_path]):
            # We explicitly prompt for a detailed review and Chromium style.
            _, output = run_gemini(
                "Review the provided patch. Ensure it uses base::span, follows "
                "Chromium idioms, is easy to read, and includes safety "
                "comments.",
                clear_out_dir=False)
    finally:
        os.remove(temp_reviewer_prompt_path)

    # Parse the reviewer output from the stream.
    review_text = ""
    for entry in output:
        if 'output' in entry:
            review_text += entry['output']

    if "CHANGES_REQUESTED" in review_text:
        return "FAILURE", review_text
    return "SUCCESS", review_text


def main():
    parser = argparse.ArgumentParser(
        description='Run Gemini to fix unsafe buffer usage in a file.')
    parser.add_argument('file', type=str, help='Path to the file to process.')
    args = parser.parse_args()
    print(f"Processing {args.file}...")

    ensure_gn_build_dir()
    ensure_docs()

    feedback = ""
    success = False

    for i in range(1, 11):
        print(f"\n--- Iteration {i} ---")

        # Entity 1: Generator
        # Construct the prompt and write it to a temporary file to ensure
        # the agent sees it via the GEMINI.md context.
        prompt_content = f"Fix the unsafe buffer usage in {args.file}."
        if feedback:
            prompt_content += f"\n\nPrevious feedback:\n{feedback}"

        with tempfile.NamedTemporaryFile(mode='w',
                                         suffix='.md',
                                         delete=False,
                                         encoding='utf-8') as temp_f:
            temp_f.write(prompt_content)
            temp_prompt_path = temp_f.name

        print("Running Generator Agent...")
        try:
            with setup_gemini_context_md(
                [FIX_PROMPT_MD, args.file, temp_prompt_path]):
                exit_code, _ = run_gemini(prompt_content)
                if exit_code != 0:
                    feedback = (
                        f"Generator agent failed with exit code {exit_code}. "
                        "Please try again."
                    )
                    continue
        finally:
            os.remove(temp_prompt_path)

        # After the generator finishes, format the code to ensure a clean diff
        # and to catch basic syntax errors before Entity 2.
        print("Formatting changes with git cl format...")
        format_result = subprocess.run(['git', 'cl', 'format'],
                                       capture_output=True,
                                       text=True,
                                       check=False)
        if format_result.returncode != 0:
            feedback = (
                "git cl format failed (likely a syntax error):\n"
                f"{format_result.stderr}"
            )
            continue

        # Entity 2: Deterministic Checker (CQ simulation)
        compile_errors = run_deterministic_check(args.file)
        if compile_errors:
            feedback = "CQ (Compilation/Tests) failed:\n" + "\n".join(
                compile_errors)
            continue

        # Entity 3: Reviewer
        review_status, review_feedback = run_reviewer(args.file)
        if review_status == "FAILURE":
            feedback = f"Reviewer requested changes:\n{review_feedback}"
            continue

        print("Patch approved by reviewer and passed CQ simulation!")
        success = True
        break
    else:
        print("Reached maximum iterations (10) without success.")

    # Finalize outputs
    # Read the summary.json file generated by gemini.
    summary = {}
    if os.path.exists(SUMMARY_PATH):
        with open(SUMMARY_PATH, 'r', encoding='utf-8') as f:
            summary = json.load(f)

    diff_result = subprocess.run(['git', 'diff', 'HEAD', '--', args.file],
                                 capture_output=True,
                                 text=True,
                                 check=False)

    # The commit message is written by gemini to a file.
    commit_message = None
    if os.path.exists(COMMIT_MESSAGE_PATH):
        with open(COMMIT_MESSAGE_PATH, 'r', encoding='utf-8') as f:
            commit_message = f.read()

    output_data = {
        'commit_message': commit_message,
        'diff': diff_result.stdout,
        'file': args.file,
        'status': "SUCCESS" if success else "FAILURE",
        'summary': summary,
        'iterations': i,
    }

    with open('gemini_spanification_output.json', 'w', encoding='utf-8') as f:
        json.dump(output_data, f, indent=2)


if __name__ == '__main__':
    main()
