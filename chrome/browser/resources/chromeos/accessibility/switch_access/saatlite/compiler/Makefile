parser: parser.jison lexer.jisonlex
	npx jison-gho parser.jison lexer.jisonlex -o parser.js
	./format_parser.sh

clean:
	rm -f parser.js
