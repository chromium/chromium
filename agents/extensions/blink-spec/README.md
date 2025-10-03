# Blink Spec MCP Server

This MCP server allows gemini-cli to fetch github comments for a particular
issue. It also allows mapping from a spec body to the github URL where issues
are located.

## Installation

```
$ agents/extensions/install.py add blink_spec
```

## Setup

In order to use the github API, each person who installs the extension is
required to create a personal access token:

Direct link: https://github.com/settings/personal-access-tokens
Or:
- In github, click your avatar at the top-right,
- Click `Settings`
- Click `Developer Settings`
- Under `Personal access tokens`, navigate to `Fine-grained tokens`


- Create a token and save it somewhere.
  - Note that some groups (w3c) require that the token has an expiration time
    less than 366 days.
  - Note that at least read-only access is required for Issues and Pull
    Requests.

Add the access token as a `BLINK_SPEC_GITHUB_API_KEY` environment variable prior
to invoking gemini:
```
$ echo "export BLINK_SPEC_GITHUB_API_KEY=your_key" >> ~/.bashrc
$ source ~/.bashrc
```

## Usage

Sample query (after starting gemini-cli):
```
> summarize css issue 12336
```

(Googlers only)
Note that if the extension is blocked due to not being allowlisted, you can try
it by invoking gemini with a mcpdev flag:
```
$ gemini --mcpdev
```

TODO(https://issues.chromium.org/439574172): Allowlist this mcp.
