# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import subprocess
import os
import sys
import json
from datetime import datetime
import threading

# --- Constants for File Paths ---
RUN_OUTPUTS_DIR = os.path.expanduser(os.path.join('~',
                                                  'promptfoo_run_outputs'))
GEMINI_LOG_FILE = "gemini.log"
GIT_DIFF_FILE = "git_diff.txt"
DEFAULT_TIMEOUT_SECONDS = 600


def _stream_reader(stream, log_file, full_output_list, all_output_list):
    """Reads a stream line-by-line and logs it."""
    try:
        for line in iter(stream.readline, ''):
            sys.stderr.write(line)
            sys.stderr.flush()
            log_file.write(line)
            full_output_list.append(line)
            all_output_list.append(line)
    finally:
        stream.close()


def call_api(prompt: str, options: dict, context: dict):
    """
    A promptfoo provider that runs 'gemini -y', saves artifacts, and streams
    its output with a reliable timeout.
    """
    provider_config = options.get('config', {})

    # --- 1. Set up unique directory for this run ---
    run_id = datetime.now().strftime('%Y-%m-%d_%H%M%S')
    run_dir = os.path.join(RUN_OUTPUTS_DIR, run_id)
    os.makedirs(run_dir, exist_ok=True)

    log_file_path = os.path.join(run_dir, GEMINI_LOG_FILE)
    output_file_path = os.path.join(run_dir, GIT_DIFF_FILE)

    # --- 2. Get prompts and config ---
    system_prompt = provider_config.get('system_prompt', '')
    combined_input = f"{system_prompt}\n\n{prompt}"

    try:
        timeout_seconds = int(
            provider_config.get("timeoutSeconds", DEFAULT_TIMEOUT_SECONDS))
    except (ValueError, TypeError):
        timeout_seconds = DEFAULT_TIMEOUT_SECONDS

    # --- 3. Execute and stream the command with a timeout ---
    process = None
    try:
        process = subprocess.Popen(["gemini", "-y"],
                                   stdin=subprocess.PIPE,
                                   stdout=subprocess.PIPE,
                                   stderr=subprocess.STDOUT,
                                   text=True,
                                   universal_newlines=True)
        process.stdin.write(combined_input)
        process.stdin.close()

        print(
            f"--- Streaming Gemini Output (Run ID: {run_id}, Timeout: {timeout_seconds}s) ---",
            file=sys.stderr)

        with open(log_file_path, "w") as log_f:
            log_f.write("--- System Prompt ---\n")
            log_f.write(system_prompt)
            log_f.write("\n\n--- User Prompt ---\n")
            log_f.write(prompt)
            log_f.write("\n\n--- Gemini Output ---\n")

            stdout_output = []
            all_output = []

            stdout_thread = threading.Thread(target=_stream_reader,
                                             args=(process.stdout, log_f,
                                                   stdout_output, all_output))

            stdout_thread.start()

            # Wait for the process to finish, with a timeout.
            process.wait(timeout=timeout_seconds)

            # Wait for the stream readers to finish *before* the log file is closed.
            stdout_thread.join()

        print("\n--- End of Stream ---", file=sys.stderr)

        if process.returncode != 0:
            return {
                "error":
                f"'gemini -y' failed with return code {process.returncode}. Output:\n{''.join(all_output)}"
            }

        sys.stderr.write('All output: %s\n' % all_output)

        return {"output": all_output}

    except subprocess.TimeoutExpired:
        if process:
            process.kill()
        return {"error": f"Command timed out after {timeout_seconds} seconds."}
    except Exception as e:
        return {"error": f"An unexpected error occurred: {e}"}
