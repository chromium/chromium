# summarization

https://huggingface.co/facebook/bart-large-cnn

### Javascript
```js
chrome.Malabr.SummarizationModel.init()
chrome.Malabr.SummarizationModel.infer(
		{context: "", max_len: "", min_len: ""}, (summary, error)=>{})
```

### CPP
```cpp
chrome.Malabr.SummarizationModel.init()
- label define = SUMMARIZATION_INIT


chrome.Malabr.SummarizationModel.infer
- the arg[0] is a dictonary
- label define = SUMMARIZATION_INFER

```


### Python
```python
SUMMARIZATION_INIT

summarizer = pipeline("summarization", model="facebook/bart-large-cnn")

SUMMARIZATION_INFER

summary = summarizer(ARTICLE, max_length=130, min_length=30, do_sample=False)

```