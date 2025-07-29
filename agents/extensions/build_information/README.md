# Chromium Build Information

This is a Python MCP server which provides information related to Chromium
builds. This includes relevant information about the host, such as OS and
architecture, as well as information about build directories, such as which
build directories are valid for a certain configuration.

Without this information, LLMs are tend to assume things, often incorrectly,
such as whether an output directory exists at all or whether it will compile
for the current host.

# Sample Prompts

Here are a handful of sample prompts to use the server directly or include in
additional context to direct an LLM towards using the server.

## Direct Usage

```
What is the architecture you're currently on?
```

```
What would the target_os argument be if compiling for the current host?
```

```
What output directories exist for compiling Linux/x64?
```

## Indirect Usage

Note that LLMs do not always follow directions, so these are not guaranteed to
get an LLM to use the server as part of its workflow. While these will help
guide it in the right direction, the user may need to stop the LLM and correct
it at times.

```
Unless I have specified otherwise, assume that any compilations should be done
targeting the current host's OS and architecture.
```

```
Before compiling anything, ensure that the output directory exists and is set
up to compile for the configuration that you want.
```