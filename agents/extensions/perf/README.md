# Chrome Performance Extension

This extension provides tools for interacting with the Chrome Performance MCP
server.

The server provides the following tools:

-   **bisect**: Triggers a Pinpoint bisect job to find the commit that
    introduced a performance regression within a given range of commits.

-   **GetAnomalies**: Fetches a list of untriaged performance regressions
    (Anomalies) for a specific area of the codebase (Sheriff Config).

-   **GetBodyForCommitHash**: Retrieves the full commit message body for a given
    git commit hash from a GoogleSource repository.

-   **GetChartURL**: Generates a URL to the performance graph for a specific
    anomaly.

-   **GetCommitInfoForRevisionRange**: Retrieves commit metadata (like hash and
    summary) for a given range of Chromium revision numbers.

-   **GetPerfData**: Retrieves raw performance metric data for a given set of
    tests over a specified time period.

-   **GetPerfParams**: Fetches the available parameters that can be used to
    query for performance data.

-   **GetSheriffConfigNames**: Lists all available Sheriff Config names, which
    represent different areas of performance monitoring.

-   **ListBenchmarks**: Lists all supported benchmarks available for Pinpoint
    performance testing.

-   **ListBotConfigurations**: Lists the available machine configurations (bots)
    for running Pinpoint performance tests.

-   **ListStories**: Lists the available user scenarios (stories) for a given
    performance benchmark.

-   **try**: Triggers a Pinpoint try job to run an A/B performance test,
    comparing a base commit against a commit with experimental changes.
