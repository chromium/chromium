Register PerformSearchToolRequest (perform_search) in actor::ToolRegistry for crbug.com/394234567.

Per the issue description:
- Drop the legacy `new_tab` parameter and expose only the required `query` string parameter.
- Follow the standard 4-file actor tool registration pattern.
- Do not attempt to build or run tests, and do not run git commit or upload.
