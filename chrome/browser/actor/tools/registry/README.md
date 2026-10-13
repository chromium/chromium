# actor::ToolRegistry

## Overview & Purpose

`ToolRegistry` lives inside `ActorKeyedService` (`actor_service->tool_registry()`). It maintains an in-process catalog answering what browser actions the current Chrome binary can execute, providing a single source of truth for model-facing tool definitions (`ToolDefinition`) and JSON argument schemas.

## Core Architecture & Classes

The registry subsystem centers around four core classes:

* `ToolRegistry`: Container managing registered tool definitions. It automatically discovers registered tool definitions at compile time via template expansion over `ToolRequestVariant`.
* `ToolDefinition`: Value type holding a `ToolId`, a model-facing name, a prompt description, and a `base::DictValue` JSON Schema for input parameters.
* `ToolDefinitionBuilder`: Fluent builder for constructing `ToolDefinition` instances. It defaults to an empty object schema when parameter declarations are omitted.
* `ToolSchemaBuilder`: Builder for JSON Schema parameter dictionaries.

## Tool Registration Steps

Registering an existing `ToolRequest` in `ToolRegistry` involves updating the following files:

1. `chrome/browser/actor/tools/registry/tool_definition.h`: Append `k<Name>` to `enum class ToolId` before `kMaxValue`, and update `kMaxValue = k<Name>`. Entries are append-only to preserve stable numerical identifiers.
2. `chrome/browser/actor/tools/<stem>_tool_request.h`: Declare `static constexpr std::string_view kModelFacingName = "<name>";`, parameter key constants, and `static std::optional<ToolDefinition> GetToolDefinition();`.
3. `chrome/browser/actor/tools/<stem>_tool_request.cc`: Define user-facing descriptions in an anonymous namespace, then implement `GetToolDefinition()` using `ToolDefinitionBuilder` and `ToolSchemaBuilder`.
4. `chrome/browser/actor/tools/registry/tool_registry_unittest.cc`: Add schema verification unit tests using `HasParamOfType`, `RequiresParam`, or an empty dictionary check. Also verify registry catalog containment via `GetAllTools()`.

```cpp
// Example: GetToolDefinition() implementation in <stem>_tool_request.cc
std::optional<ToolDefinition> FooToolRequest::GetToolDefinition() {
  return ToolDefinitionBuilder(ToolId::kFoo, kModelFacingName,
                               kFooToolDescription)
      .SetToolParameterSchema(ToolSchemaBuilder().AddStringProperty(
          kUrlParam, kUrlParamDescription, ToolSchemaBuilder::kFormatUri))
      .Build();
}
```

## Agent Skill Assistance

The repository includes a dedicated agent skill that assists with the tool registration process:

* Skill definition: [actor-tool-registration](../../../../../agents/skills/actor-tool-registration/SKILL.md)
* Setup command: `vpython3 agents/skills/setup.py link actor-tool-registration` (links into `.agents/skills/` for local agent discovery)

## Exemplars

Refer to the following canonical implementations:

* Tool with parameters: [crrev.com/c/8477400](https://crrev.com/c/8477400) (`NavigateToolRequest` in `chrome/browser/actor/tools/navigate_tool_request.{h,cc}`).
* Tool without parameters: [crrev.com/c/8526144](https://crrev.com/c/8526144) (`ExitFullscreenToolRequest` in `chrome/browser/actor/tools/window_management_tool_request.{h,cc}`).
