// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"

#include <ostream>

#include "base/containers/contains.h"
#include "base/containers/span.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "base/ranges/algorithm.h"
#include "base/stl_util.h"
#include "base/strings/strcat.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_split.h"
#include "base/strings/string_tokenizer.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"

#if defined(OS_WIN)
#include <windows.h>
#include <shellapi.h>

#include "base/strings/string_util_win.h"
#endif  // defined(OS_WIN)

namespace base {

CommandLine* CommandLine::current_process_commandline_ = nullptr;

namespace {

constexpr CommandLine::CharType kSwitchTerminator[] = FILE_PATH_LITERAL("--");
constexpr CommandLine::CharType kSwitchValueSeparator[] =
    FILE_PATH_LITERAL("=");

// Since we use a lazy match, make sure that longer versions (like "--") are
// listed before shorter versions (like "-") of similar prefixes.
#if defined(OS_WIN)
// By putting slash last, we can control whether it is treaded as a switch
// value by changing the value of switch_prefix_count to be one less than
// the array size.
constexpr CommandLine::StringPieceType kSwitchPrefixes[] = {L"--", L"-", L"/"};
#elif defined(OS_POSIX) || defined(OS_FUCHSIA)
// Unixes don't use slash as a switch.
constexpr CommandLine::StringPieceType kSwitchPrefixes[] = {"--", "-"};
#endif
size_t switch_prefix_count = base::size(kSwitchPrefixes);

#if defined(OS_WIN)
// Switch string that specifies the single argument to the command line.
// If present, everything after this switch is interpreted as a single
// argument regardless of whitespace, quotes, etc. Used for launches from the
// Windows shell, which may have arguments with unencoded quotes that could
// otherwise unexpectedly be split into multiple arguments
// (https://crbug.com/937179).
constexpr CommandLine::CharType kSingleArgument[] =
    FILE_PATH_LITERAL("single-argument");
#endif  // defined(OS_WIN)

size_t GetSwitchPrefixLength(CommandLine::StringPieceType string) {
  for (size_t i = 0; i < switch_prefix_count; ++i) {
    CommandLine::StringType prefix(kSwitchPrefixes[i]);
    if (string.substr(0, prefix.length()) == prefix)
      return prefix.length();
  }
  return 0;
}

// Fills in |switch_string| and |switch_value| if |string| is a switch.
// This will preserve the input switch prefix in the output |switch_string|.
bool IsSwitch(const CommandLine::StringType& string,
              CommandLine::StringType* switch_string,
              CommandLine::StringType* switch_value) {
  switch_string->clear();
  switch_value->clear();
  size_t prefix_length = GetSwitchPrefixLength(string);
  if (prefix_length == 0 || prefix_length == string.length())
    return false;

  const size_t equals_position = string.find(kSwitchValueSeparator);
  *switch_string = string.substr(0, equals_position);
  if (equals_position != CommandLine::StringType::npos)
    *switch_value = string.substr(equals_position + 1);
  return true;
}

// Returns true iff |string| represents a switch with key
// |switch_key_without_prefix|, regardless of value.
bool IsSwitchWithKey(CommandLine::StringPieceType string,
                     CommandLine::StringPieceType switch_key_without_prefix) {
  size_t prefix_length = GetSwitchPrefixLength(string);
  if (prefix_length == 0 || prefix_length == string.length())
    return false;

  const size_t equals_position = string.find(kSwitchValueSeparator);
  return string.substr(prefix_length, equals_position - prefix_length) ==
         switch_key_without_prefix;
}

#if defined(OS_WIN)
// Quote a string as necessary for CommandLineToArgvW compatibility *on
// Windows*.
std::wstring QuoteForCommandLineToArgvW(const std::wstring& arg,
                                        bool allow_unsafe_insert_sequences) {
  // Ensure that GetCommandLineString isn't used to generate command-line
  // strings for the Windows shell by checking for Windows insert sequences like
  // "%1". GetCommandLineStringForShell should be used instead to get a string
  // with the correct placeholder format for the shell.
  DCHECK(arg.size() != 2 || arg[0] != L'%' || allow_unsafe_insert_sequences);

  // We follow the quoting rules of CommandLineToArgvW.
  // http://msdn.microsoft.com/en-us/library/17w5ykft.aspx
  std::wstring quotable_chars(L" \\\"");
  if (arg.find_first_of(quotable_chars) == std::wstring::npos) {
    // No quoting necessary.
    return arg;
  }

  std::wstring out;
  out.push_back('"');
  for (size_t i = 0; i < arg.size(); ++i) {
    if (arg[i] == '\\') {
      // Find the extent of this run of backslashes.
      size_t start = i, end = start + 1;
      for (; end < arg.size() && arg[end] == '\\'; ++end) {}
      size_t backslash_count = end - start;

      // Backslashes are escapes only if the run is followed by a double quote.
      // Since we also will end the string with a double quote, we escape for
      // either a double quote or the end of the string.
      if (end == arg.size() || arg[end] == '"') {
        // To quote, we need to output 2x as many backslashes.
        backslash_count *= 2;
      }
      for (size_t j = 0; j < backslash_count; ++j)
        out.push_back('\\');

      // Advance i to one before the end to balance i++ in loop.
      i = end - 1;
    } else if (arg[i] == '"') {
      out.push_back('\\');
      out.push_back('"');
    } else {
      out.push_back(arg[i]);
    }
  }
  out.push_back('"');

  return out;
}
#endif  // defined(OS_WIN)

}  // namespace

CommandLine::CommandLine(NoProgram no_program)
    : argv_(1),
      begin_args_(1) {
}

CommandLine::CommandLine(const FilePath& program)
    : argv_(1),
      begin_args_(1) {
  SetProgram(program);
}

CommandLine::CommandLine(int argc, const CommandLine::CharType* const* argv)
    : argv_(1),
      begin_args_(1) {
  InitFromArgv(argc, argv);
}

CommandLine::CommandLine(const StringVector& argv)
    : argv_(1),
      begin_args_(1) {
  InitFromArgv(argv);
}

CommandLine::CommandLine(const CommandLine& other) = default;

CommandLine& CommandLine::operator=(const CommandLine& other) = default;

CommandLine::~CommandLine() = default;

#if defined(OS_WIN)
// static
void CommandLine::set_slash_is_not_a_switch() {
  // The last switch prefix should be slash, so adjust the size to skip it.
  static_assert(base::make_span(kSwitchPrefixes).back() == L"/",
                "Error: Last switch prefix is not a slash.");
  switch_prefix_count = base::size(kSwitchPrefixes) - 1;
}

// static
void CommandLine::InitUsingArgvForTesting(int argc, const char* const* argv) {
  DCHECK(!current_process_commandline_);
  current_process_commandline_ = new CommandLine(NO_PROGRAM);
  // On Windows we need to convert the command line arguments to std::wstring.
  CommandLine::StringVector argv_vector;
  for (int i = 0; i < argc; ++i)
    argv_vector.push_back(UTF8ToWide(argv[i]));
  current_process_commandline_->InitFromArgv(argv_vector);
}
#endif  // defined(OS_WIN)

// static
bool CommandLine::Init(int argc, const char* const* argv) {
  if (current_process_commandline_) {
    // If this is intentional, Reset() must be called first. If we are using
    // the shared build mode, we have to share a single object across multiple
    // shared libraries.
    return false;
  }

  current_process_commandline_ = new CommandLine(NO_PROGRAM);
#if defined(OS_WIN)
  current_process_commandline_->ParseFromString(::GetCommandLineW());
#elif defined(OS_POSIX) || defined(OS_FUCHSIA)
  current_process_commandline_->InitFromArgv(argc, argv);
#else
#error Unsupported platform
#endif

  return true;
}

// static
void CommandLine::Reset() {
  DCHECK(current_process_commandline_);
  delete current_process_commandline_;
  current_process_commandline_ = nullptr;
}

// static
CommandLine* CommandLine::ForCurrentProcess() {
  DCHECK(current_process_commandline_);
  return current_process_commandline_;
}

// static
bool CommandLine::InitializedForCurrentProcess() {
  return !!current_process_commandline_;
}

#if defined(OS_WIN)
// static
CommandLine CommandLine::FromString(StringPieceType command_line) {
  CommandLine cmd(NO_PROGRAM);
  cmd.ParseFromString(command_line);
  return cmd;
}
#endif  // defined(OS_WIN)

void CommandLine::InitFromArgv(int argc,
                               const CommandLine::CharType* const* argv) {
  StringVector new_argv;
  for (int i = 0; i < argc; ++i)
    new_argv.push_back(argv[i]);
  InitFromArgv(new_argv);
}

void CommandLine::InitFromArgv(const StringVector& argv) {
  argv_ = StringVector(1);
  switches_.clear();
  begin_args_ = 1;
  SetProgram(argv.empty() ? FilePath() : FilePath(argv[0]));
  AppendSwitchesAndArguments(argv);
}

FilePath CommandLine::GetProgram() const {
  return FilePath(argv_[0]);
}

void CommandLine::SetProgram(const FilePath& program) {
#if defined(OS_WIN)
  argv_[0] = StringType(TrimWhitespace(program.value(), TRIM_ALL));
#elif defined(OS_POSIX) || defined(OS_FUCHSIA)
  TrimWhitespaceASCII(program.value(), TRIM_ALL, &argv_[0]);
#else
#error Unsupported platform
#endif
}

bool CommandLine::HasSwitch(StringPiece switch_string) const {
  DCHECK_EQ(ToLowerASCII(switch_string), switch_string);
  return Contains(switches_, switch_string);
}

bool CommandLine::HasSwitch(const char switch_constant[]) const {
  return HasSwitch(StringPiece(switch_constant));
}

std::string CommandLine::GetSwitchValueASCII(StringPiece switch_string) const {
  StringType value = GetSwitchValueNative(switch_string);
#if defined(OS_WIN)
  if (!IsStringASCII(base::AsStringPiece16(value))) {
#elif defined(OS_POSIX) || defined(OS_FUCHSIA)
  if (!IsStringASCII(value)) {
#endif
    DLOG(WARNING) << "Value of switch (" << switch_string << ") must be ASCII.";
    return std::string();
  }
#if defined(OS_WIN)
  return WideToUTF8(value);
#elif defined(OS_POSIX) || defined(OS_FUCHSIA)
  return value;
#endif
}

FilePath CommandLine::GetSwitchValuePath(StringPiece switch_string) const {
  return FilePath(GetSwitchValueNative(switch_string));
}

CommandLine::StringType CommandLine::GetSwitchValueNative(
    StringPiece switch_string) const {
  DCHECK_EQ(ToLowerASCII(switch_string), switch_string);
  auto result = switches_.find(switch_string);
  return result == switches_.end() ? StringType() : result->second;
}

void CommandLine::AppendSwitch(StringPiece switch_string) {
  AppendSwitchNative(switch_string, StringType());
}

void CommandLine::AppendSwitchPath(StringPiece switch_string,
                                   const FilePath& path) {
  AppendSwitchNative(switch_string, path.value());
}

void CommandLine::AppendSwitchNative(StringPiece switch_string,
                                     CommandLine::StringPieceType value) {
#if defined(OS_WIN)
  const std::string switch_key = ToLowerASCII(switch_string);
  StringType combined_switch_string(UTF8ToWide(switch_key));
#elif defined(OS_POSIX) || defined(OS_FUCHSIA)
  StringPiece switch_key = switch_string;
  StringType combined_switch_string(switch_key);
#endif
  size_t prefix_length = GetSwitchPrefixLength(combined_switch_string);
  base::InsertOrAssign(switches_, std::string(switch_key.substr(prefix_length)),
                       StringType(value));
  // Preserve existing switch prefixes in |argv_|; only append one if necessary.
  if (prefix_length == 0) {
    combined_switch_string.insert(0, kSwitchPrefixes[0].data(),
                                  kSwitchPrefixes[0].size());
  }
  if (!value.empty())
    base::StrAppend(&combined_switch_string, {kSwitchValueSeparator, value});
  // Append the switch and update the switches/arguments divider |begin_args_|.
  argv_.insert(argv_.begin() + begin_args_++, combined_switch_string);
}

void CommandLine::AppendSwitchASCII(StringPiece switch_string,
                                    StringPiece value_string) {
#if defined(OS_WIN)
  AppendSwitchNative(switch_string, UTF8ToWide(value_string));
#elif defined(OS_POSIX) || defined(OS_FUCHSIA)
  AppendSwitchNative(switch_string, value_string);
#else
#error Unsupported platform
#endif
}

void CommandLine::RemoveSwitch(base::StringPiece switch_key_without_prefix) {
#if defined(OS_WIN)
  StringType switch_key_native = UTF8ToWide(switch_key_without_prefix);
#elif defined(OS_POSIX) || defined(OS_FUCHSIA)
  StringType switch_key_native(switch_key_without_prefix);
#endif

  DCHECK_EQ(ToLowerASCII(switch_key_without_prefix), switch_key_without_prefix);
  DCHECK_EQ(0u, GetSwitchPrefixLength(switch_key_native));
  auto it = switches_.find(switch_key_without_prefix);
  if (it == switches_.end())
    return;
  switches_.erase(it);
  // Also erase from the switches section of |argv_| and update |begin_args_|
  // accordingly.
  // Switches in |argv_| have indices [1, begin_args_).
  auto argv_switches_begin = argv_.begin() + 1;
  auto argv_switches_end = argv_.begin() + begin_args_;
  DCHECK(argv_switches_begin <= argv_switches_end);
  DCHECK(argv_switches_end <= argv_.end());
  auto expell = std::remove_if(argv_switches_begin, argv_switches_end,
                               [&switch_key_native](const StringType& arg) {
                                 return IsSwitchWithKey(arg, switch_key_native);
                               });
  if (expell == argv_switches_end) {
    NOTREACHED();
    return;
  }
  begin_args_ -= argv_switches_end - expell;
  argv_.erase(expell, argv_switches_end);
}

void CommandLine::CopySwitchesFrom(const CommandLine& source,
                                   const char* const switches[],
                                   size_t count) {
  for (size_t i = 0; i < count; ++i) {
    if (source.HasSwitch(switches[i]))
      AppendSwitchNative(switches[i], source.GetSwitchValueNative(switches[i]));
  }
}

CommandLine::StringVector CommandLine::GetArgs() const {
  // Gather all arguments after the last switch (may include kSwitchTerminator).
  StringVector args(argv_.begin() + begin_args_, argv_.end());
  // Erase only the first kSwitchTerminator (maybe "--" is a legitimate page?)
  auto switch_terminator = ranges::find(args, kSwitchTerminator);
  if (switch_terminator != args.end())
    args.erase(switch_terminator);
  return args;
}

void CommandLine::AppendArg(StringPiece value) {
#if defined(OS_WIN)
  DCHECK(IsStringUTF8(value));
  AppendArgNative(UTF8ToWide(value));
#elif defined(OS_POSIX) || defined(OS_FUCHSIA)
  AppendArgNative(value);
#else
#error Unsupported platform
#endif
}

void CommandLine::AppendArgPath(const FilePath& path) {
  AppendArgNative(path.value());
}

void CommandLine::AppendArgNative(StringPieceType value) {
  argv_.push_back(StringType(value));
}

void CommandLine::AppendArguments(const CommandLine& other,
                                  bool include_program) {
  if (include_program)
    SetProgram(other.GetProgram());
  AppendSwitchesAndArguments(other.argv());
}

void CommandLine::PrependWrapper(StringPieceType wrapper) {
  if (wrapper.empty())
    return;
  // Split the wrapper command based on whitespace (with quoting).
  // StringPieceType does not currently work directly with StringTokenizerT.
  using CommandLineTokenizer =
      StringTokenizerT<StringType, StringType::const_iterator>;
  StringType wrapper_string(wrapper);
  CommandLineTokenizer tokenizer(wrapper_string, FILE_PATH_LITERAL(" "));
  tokenizer.set_quote_chars(FILE_PATH_LITERAL("'\""));
  std::vector<StringType> wrapper_argv;
  while (tokenizer.GetNext())
    wrapper_argv.emplace_back(tokenizer.token());

  // Prepend the wrapper and update the switches/arguments |begin_args_|.
  argv_.insert(argv_.begin(), wrapper_argv.begin(), wrapper_argv.end());
  begin_args_ += wrapper_argv.size();
}

#if defined(OS_WIN)
void CommandLine::ParseFromString(StringPieceType command_line) {
  command_line = TrimWhitespace(command_line, TRIM_ALL);
  if (command_line.empty())
    return;
  raw_command_line_string_ = command_line;

  int num_args = 0;
  wchar_t** args = NULL;
  // When calling CommandLineToArgvW, use the apiset if available.
  // Doing so will bypass loading shell32.dll on Win8+.
  HMODULE downlevel_shell32_dll =
      ::LoadLibraryEx(L"api-ms-win-downlevel-shell32-l1-1-0.dll", nullptr,
                      LOAD_LIBRARY_SEARCH_SYSTEM32);
  if (downlevel_shell32_dll) {
    auto command_line_to_argv_w_proc =
        reinterpret_cast<decltype(::CommandLineToArgvW)*>(
            ::GetProcAddress(downlevel_shell32_dll, "CommandLineToArgvW"));
    if (command_line_to_argv_w_proc)
      args = command_line_to_argv_w_proc(command_line.data(), &num_args);
  } else {
    // Since the apiset is not available, allow the delayload of shell32.dll
    // to take place.
    args = ::CommandLineToArgvW(command_line.data(), &num_args);
  }

  DPLOG_IF(FATAL, !args) << "CommandLineToArgvW failed on command line: "
                         << command_line;
  StringVector argv(args, args + num_args);
  InitFromArgv(argv);
  raw_command_line_string_ = StringPieceType();
  LocalFree(args);

  if (downlevel_shell32_dll)
    ::FreeLibrary(downlevel_shell32_dll);
}
#endif  // defined(OS_WIN)

void CommandLine::AppendSwitchesAndArguments(
    const CommandLine::StringVector& argv) {
  bool parse_switches = true;
#if defined(OS_WIN)
  const bool is_parsed_from_string = !raw_command_line_string_.empty();
#endif
  for (size_t i = 1; i < argv.size(); ++i) {
    CommandLine::StringType arg = argv[i];
#if defined(OS_WIN)
    arg = CommandLine::StringType(TrimWhitespace(arg, TRIM_ALL));
#elif defined(OS_POSIX) || defined(OS_FUCHSIA)
    TrimWhitespaceASCII(arg, TRIM_ALL, &arg);
#endif

    CommandLine::StringType switch_string;
    CommandLine::StringType switch_value;
    parse_switches &= (arg != kSwitchTerminator);
    if (parse_switches && IsSwitch(arg, &switch_string, &switch_value)) {
#if defined(OS_WIN)
      if (is_parsed_from_string &&
          IsSwitchWithKey(switch_string, kSingleArgument)) {
        ParseAsSingleArgument(switch_string);
        return;
      }
      AppendSwitchNative(WideToUTF8(switch_string), switch_value);
#elif defined(OS_POSIX) || defined(OS_FUCHSIA)
      AppendSwitchNative(switch_string, switch_value);
#else
#error Unsupported platform
#endif
    } else {
      AppendArgNative(arg);
    }
  }
}

CommandLine::StringType CommandLine::GetArgumentsStringInternal(
    bool allow_unsafe_insert_sequences) const {
  StringType params;
  // Append switches and arguments.
  bool parse_switches = true;
  for (size_t i = 1; i < argv_.size(); ++i) {
    StringType arg = argv_[i];
    StringType switch_string;
    StringType switch_value;
    parse_switches &= arg != kSwitchTerminator;
    if (i > 1)
      params.append(FILE_PATH_LITERAL(" "));
    if (parse_switches && IsSwitch(arg, &switch_string, &switch_value)) {
      params.append(switch_string);
      if (!switch_value.empty()) {
#if defined(OS_WIN)
        switch_value = QuoteForCommandLineToArgvW(
            switch_value, allow_unsafe_insert_sequences);
#endif
        params.append(kSwitchValueSeparator + switch_value);
      }
    } else {
#if defined(OS_WIN)
      arg = QuoteForCommandLineToArgvW(arg, allow_unsafe_insert_sequences);
#endif
      params.append(arg);
    }
  }
  return params;
}

CommandLine::StringType CommandLine::GetCommandLineString() const {
  StringType string(argv_[0]);
#if defined(OS_WIN)
  string = QuoteForCommandLineToArgvW(string,
                                      /*allow_unsafe_insert_sequences=*/false);
#endif
  StringType params(GetArgumentsString());
  if (!params.empty()) {
    string.append(FILE_PATH_LITERAL(" "));
    string.append(params);
  }
  return string;
}

#if defined(OS_WIN)
// NOTE: this function is used to set Chrome's open command in the registry
// during update. Any change to the syntax must be compatible with the prior
// version (i.e., any new syntax must be understood by older browsers expecting
// the old syntax, and the new browser must still handle the old syntax), as
// old versions are likely to persist, e.g., immediately after background
// update, when parsing command lines for other channels, when uninstalling web
// applications installed using the old syntax, etc.
CommandLine::StringType CommandLine::GetCommandLineStringForShell() const {
  DCHECK(GetArgs().empty());
  StringType command_line_string = GetCommandLineString();
  return command_line_string + FILE_PATH_LITERAL(" ") +
         StringType(kSwitchPrefixes[0]) + kSingleArgument +
         FILE_PATH_LITERAL(" %1");
}

CommandLine::StringType
CommandLine::GetCommandLineStringWithUnsafeInsertSequences() const {
  StringType string(argv_[0]);
  string = QuoteForCommandLineToArgvW(string,
                                      /*allow_unsafe_insert_sequences=*/true);
  StringType params(
      GetArgumentsStringInternal(/*allow_unsafe_insert_sequences=*/true));
  if (!params.empty()) {
    string.append(FILE_PATH_LITERAL(" "));
    string.append(params);
  }
  return string;
}
#endif  // defined(OS_WIN)

CommandLine::StringType CommandLine::GetArgumentsString() const {
  return GetArgumentsStringInternal(/*allow_unsafe_insert_sequences=*/false);
}

#if defined(OS_WIN)
void CommandLine::ParseAsSingleArgument(
    const CommandLine::StringType& single_arg_switch) {
  DCHECK(!raw_command_line_string_.empty());

  // Remove any previously parsed arguments.
  argv_.resize(begin_args_);

  // Locate "--single-argument" in the process's raw command line. Results are
  // unpredictable if "--single-argument" appears as part of a previous
  // argument or switch.
  const size_t single_arg_switch_position =
      raw_command_line_string_.find(single_arg_switch);
  DCHECK_NE(single_arg_switch_position, StringType::npos);

  // Append the portion of the raw command line that starts one character past
  // "--single-argument" as the one and only argument, or return if no
  // argument is present.
  const size_t arg_position =
      single_arg_switch_position + single_arg_switch.length() + 1;
  if (arg_position >= raw_command_line_string_.length())
    return;
  const StringPieceType arg = raw_command_line_string_.substr(arg_position);
  if (!arg.empty()) {
    AppendArgNative(arg);
  }
}
#endif  // defined(OS_WIN)

}  // namespace base
